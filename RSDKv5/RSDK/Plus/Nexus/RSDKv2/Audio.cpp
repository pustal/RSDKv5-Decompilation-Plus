#include "RetroEngine.hpp"

// (Plus) The decompilation's Audio.cpp without SDL and libvorbisfile. The
// channels, the mixing (volumes, panning, clamping) and the scripts' view of
// it are the decompilation's; what changed:
//   - the host's audio thread pulls the mix (MixAudio()) instead of an SDL
//     audio device, under audioLock instead of SDL_LockAudio();
//   - sound effects (WAV) are read here and converted to the mixing format
//     (interleaved stereo 16-bit at AUDIO_FREQUENCY) instead of with SDL;
//   - music (Ogg Vorbis) is decoded with stb_vorbis from the file's bytes,
//     resampled as it plays, and loaded when asked rather than on a thread.

int NoGlobalSFX = 0;
int NoStageSFX  = 0;

int MusicVolume       = MAX_VOLUME;
int CurrentMusicTrack = -1;
int sfxVolume         = MAX_VOLUME;
int bgmVolume         = MAX_VOLUME;
bool audioEnabled     = false;

int nextChannelPos;
bool musicEnabled;
int musicStatus;
TrackInfo musicTracks[TRACK_COUNT];
SFXInfo sfxList[SFX_COUNT];

ChannelInfo sfxChannels[CHANNEL_COUNT];

MusicPlaybackInfo musInfo;

std::mutex audioLock;

#define ADJUST_VOLUME(s, v) (s = (s * v) / MAX_VOLUME)

#define MIX_BUFFER_SAMPLES (256)

int InitSoundDevice()
{
    StopAllSfx(); //"init"
    // (Plus) The host's device is already running and pulls MixAudio().
    audioEnabled = true;

    LoadGlobalSfx();

    return true;
}

void LoadGlobalSfx()
{
    FileInfo info;
    FileInfo infoStore;
    char strBuffer[0x100];
    byte fileBuffer = 0;
    int fileBuffer2 = 0;

    if (LoadFile("Data/Game/GameConfig.bin", &info)) {
        infoStore = info;

        FileRead(&fileBuffer, 1);
        FileRead(strBuffer, fileBuffer);
        strBuffer[fileBuffer] = 0;

        FileRead(&fileBuffer, 1);
        FileRead(&strBuffer, fileBuffer); // Load 'Data'
        strBuffer[fileBuffer] = 0;

        FileRead(&fileBuffer, 1);
        FileRead(strBuffer, fileBuffer);
        strBuffer[fileBuffer] = 0;

        // Read Script Paths
        byte scriptCount = 0;
        FileRead(&scriptCount, 1);
        for (byte s = 0; s < scriptCount; ++s) {
            FileRead(&fileBuffer, 1);
            FileRead(strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;
        }

        byte varCnt = 0;
        FileRead(&varCnt, 1);
        for (byte v = 0; v < varCnt; ++v) {
            FileRead(&fileBuffer, 1);
            FileRead(strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;

            // Read Variable Value
            FileRead(&fileBuffer2, 4);
        }

        // Read SFX
        FileRead(&fileBuffer, 1);
        NoGlobalSFX = fileBuffer;
        for (byte s = 0; s < NoGlobalSFX; ++s) {
            FileRead(&fileBuffer, 1);
            FileRead(strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;

            GetFileInfo(&infoStore);
            LoadSfx(strBuffer, s);
            SetFileInfo(&infoStore);
        }

        CloseFile();
    }

    nextChannelPos = 0;
    for (int i = 0; i < CHANNEL_COUNT; ++i) sfxChannels[i].sfxID = -1;
}

// ---------------------
// Mixing
// ---------------------

void ProcessAudioMixing(Sint32 *dst, const Sint16 *src, int len, int volume, sbyte pan)
{
    if (volume == 0)
        return;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;

    float panL = 0;
    float panR = 0;
    int i      = 0;

    if (pan < 0) {
        panR = 1.0f - fabsf(pan / 100.0f);
        panL = 1.0f;
    }
    else if (pan > 0) {
        panL = 1.0f - fabsf(pan / 100.0f);
        panR = 1.0f;
    }

    while (len--) {
        Sint32 sample = *src++;
        ADJUST_VOLUME(sample, volume);

        if (pan != 0) {
            if ((i % 2) != 0) {
                sample = (Sint32)(sample * panR);
            }
            else {
                sample = (Sint32)(sample * panL);
            }
        }

        *dst++ += sample;

        i++;
    }
}

/** (Plus) The music's next stereo frame at AUDIO_FREQUENCY, or false at its end (not looping). Under audioLock. */
bool NextMusicFrame(float *left, float *right)
{
    // Decoded a chunk at a time into musInfo.chunk (stb_vorbis duplicates nothing: mono is read as mono).
    auto readFrame = [](float *frame) {
        if (musInfo.chunkPos >= musInfo.chunkFrames) {
            const int channels = musInfo.sourceChannels >= 2 ? 2 : 1;
            int got = stb_vorbis_get_samples_float_interleaved(musInfo.vorbis, channels, musInfo.chunk, MUSIC_CHUNK_FRAMES * channels);
            if (got == 0 && musInfo.trackLoop) {
                stb_vorbis_seek_start(musInfo.vorbis);
                got = stb_vorbis_get_samples_float_interleaved(musInfo.vorbis, channels, musInfo.chunk, MUSIC_CHUNK_FRAMES * channels);
            }
            if (got == 0)
                return false;
            musInfo.chunkFrames = got;
            musInfo.chunkPos    = 0;
        }
        if (musInfo.sourceChannels >= 2) {
            frame[0] = musInfo.chunk[musInfo.chunkPos * 2];
            frame[1] = musInfo.chunk[musInfo.chunkPos * 2 + 1];
        }
        else {
            frame[0] = frame[1] = musInfo.chunk[musInfo.chunkPos];
        }
        ++musInfo.chunkPos;
        return true;
    };

    if (!musInfo.primed) {
        if (!readFrame(musInfo.frameA))
            return false;
        if (!readFrame(musInfo.frameB)) {
            musInfo.frameB[0] = musInfo.frameA[0];
            musInfo.frameB[1] = musInfo.frameA[1];
        }
        musInfo.position = 0.0;
        musInfo.primed   = true;
    }

    const double step = (double)musInfo.sourceRate / AUDIO_FREQUENCY;
    while (musInfo.position >= 1.0) {
        musInfo.frameA[0] = musInfo.frameB[0];
        musInfo.frameA[1] = musInfo.frameB[1];
        if (!readFrame(musInfo.frameB))
            return false;
        musInfo.position -= 1.0;
    }

    const float t = (float)musInfo.position;
    *left         = musInfo.frameA[0] + (musInfo.frameB[0] - musInfo.frameA[0]) * t;
    *right        = musInfo.frameA[1] + (musInfo.frameB[1] - musInfo.frameA[1]) * t;
    musInfo.position += step;
    return true;
}

/** Mixes @p samples Sint16s of music into @p stream (the decompilation's ProcessMusicStream()). Under audioLock. */
void ProcessMusicStream(Sint32 *stream, size_t samples)
{
    if (!musInfo.loaded || !musInfo.vorbis)
        return;
    switch (musicStatus) {
        case MUSIC_READY:
        case MUSIC_PLAYING: {
            Sint16 buffer[MIX_BUFFER_SAMPLES];
            size_t done = 0;
            for (; done + 1 < samples; done += 2) {
                float left = 0, right = 0;
                if (!NextMusicFrame(&left, &right)) {
                    musicStatus = MUSIC_STOPPED;
                    break;
                }
                auto toSample = [](float value) {
                    const float scaled = value * 32767.0f;
                    return (Sint16)(scaled > 32767.0f ? 32767.0f : scaled < -32768.0f ? -32768.0f : scaled);
                };
                buffer[done]     = toSample(left);
                buffer[done + 1] = toSample(right);
            }
            if (done)
                ProcessAudioMixing(stream, buffer, (int)done, (bgmVolume * MusicVolume) / MAX_VOLUME, 0);
            break;
        }
        case MUSIC_STOPPED:
        case MUSIC_PAUSED:
        case MUSIC_LOADING:
            // dont play
            break;
    }
}

void StopSfxLocked(int sfx)
{
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID == sfx) {
            MEM_ZERO(sfxChannels[i]);
            sfxChannels[i].sfxID = -1;
        }
    }
}

void MixAudio(Sint16 *output, size_t samples)
{
    std::lock_guard<std::mutex> guard(audioLock);
    if (!audioEnabled) {
        memset(output, 0, samples * sizeof(Sint16));
        return;
    }

    Sint16 *output_buffer    = output;
    size_t samples_remaining = samples;
    while (samples_remaining != 0) {
        Sint32 mix_buffer[MIX_BUFFER_SAMPLES];
        memset(mix_buffer, 0, sizeof(mix_buffer));

        const size_t samples_to_do = (samples_remaining < MIX_BUFFER_SAMPLES) ? samples_remaining : MIX_BUFFER_SAMPLES;

        // Mix music
        ProcessMusicStream(mix_buffer, samples_to_do);

        // Mix SFX
        for (byte i = 0; i < CHANNEL_COUNT; ++i) {
            ChannelInfo *sfx = &sfxChannels[i];
            if (sfx->sfxID < 0)
                continue;

            if (sfx->samplePtr) {
                Sint16 buffer[MIX_BUFFER_SAMPLES];

                size_t samples_done = 0;
                while (samples_done != samples_to_do) {
                    size_t sampleLen = (sfx->sampleLength < samples_to_do - samples_done) ? sfx->sampleLength : samples_to_do - samples_done;
                    memcpy(&buffer[samples_done], sfx->samplePtr, sampleLen * sizeof(Sint16));

                    samples_done += sampleLen;
                    sfx->samplePtr += sampleLen;
                    sfx->sampleLength -= sampleLen;

                    if (sfx->sampleLength == 0) {
                        if (sfx->loopSFX) {
                            sfx->samplePtr    = sfxList[sfx->sfxID].buffer;
                            sfx->sampleLength = sfxList[sfx->sfxID].length;
                        }
                        else {
                            StopSfxLocked(sfx->sfxID);
                            break;
                        }
                    }
                }

                ProcessAudioMixing(mix_buffer, buffer, (int)samples_done, sfxVolume, sfx->pan);
            }
        }

        // Clamp mixed samples back to 16-bit and write them to the output buffer
        for (size_t i = 0; i < samples_to_do; ++i) {
            const Sint32 max_audioval = ((1 << (16 - 1)) - 1);
            const Sint32 min_audioval = -(1 << (16 - 1));

            const Sint32 sample = mix_buffer[i];

            if (sample > max_audioval)
                *output_buffer++ = (Sint16)max_audioval;
            else if (sample < min_audioval)
                *output_buffer++ = (Sint16)min_audioval;
            else
                *output_buffer++ = (Sint16)sample;
        }

        samples_remaining -= samples_to_do;
    }
}

// ---------------------
// Music
// ---------------------

/** Frees the music track (the decompilation's freeMusInfo()). Under audioLock. */
void FreeMusicLocked()
{
    if (musInfo.vorbis)
        stb_vorbis_close(musInfo.vorbis);
    if (musInfo.loaded)
        CloseFile2(&musInfo.fileInfo);
    musInfo.vorbis      = nullptr;
    musInfo.primed      = false;
    musInfo.chunkFrames = 0;
    musInfo.chunkPos    = 0;
    musInfo.trackLoop = false;
    musInfo.loaded    = false;
    musicStatus       = MUSIC_STOPPED;
}

void StopMusic()
{
    std::lock_guard<std::mutex> guard(audioLock);
    FreeMusicLocked();
}

void LoadMusic(void *userdata)
{
    const int track = *(const int *)userdata;
    if (track < 0 || track >= TRACK_COUNT || !musicTracks[track].fileName[0]) {
        StopMusic();
        return;
    }
    TrackInfo *trackPtr = &musicTracks[track];

    // Read and open the file outside the lock: the audio thread keeps playing meanwhile.
    FileInfo file;
    MEM_ZERO(file);
    if (!LoadFile2(trackPtr->fileName, &file)) {
        StopMusic();
        return;
    }
    int error          = 0;
    stb_vorbis *vorbis = stb_vorbis_open_memory(file.fileBuffer, file.vFileSize, &error, nullptr);
    if (!vorbis) {
        PrintLog("Unable to decode music '%s' (stb_vorbis error %d)", trackPtr->fileName, error);
        CloseFile2(&file);
        StopMusic();
        return;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);

    std::lock_guard<std::mutex> guard(audioLock);
    FreeMusicLocked();
    musInfo.fileInfo       = file;
    musInfo.vorbis         = vorbis;
    musInfo.sourceRate     = info.sample_rate > 0 ? (int)info.sample_rate : AUDIO_FREQUENCY;
    musInfo.sourceChannels = info.channels;
    musInfo.primed         = false;
    musInfo.chunkFrames    = 0;
    musInfo.chunkPos       = 0;
    musInfo.trackLoop      = trackPtr->trackLoop;
    musInfo.loaded         = true;

    musicStatus       = MUSIC_PLAYING;
    MusicVolume       = MAX_VOLUME;
    CurrentMusicTrack = track;
}

void SetMusicTrack(char *filePath, byte trackID, bool loop)
{
    std::lock_guard<std::mutex> guard(audioLock);
    TrackInfo *track = &musicTracks[trackID];
    StrCopy(track->fileName, "Data/Music/");
    StrAdd(track->fileName, filePath);
    track->trackLoop = loop;
}

bool PlayMusic(int track)
{
    if (!audioEnabled)
        return false;

    if (track < 0 || track >= TRACK_COUNT) {
        StopMusic();
        return false;
    }
    // (Plus) Loaded now: the decompilation's loading thread raced the game's file reading.
    musicStatus = MUSIC_LOADING;
    LoadMusic(&track);
    return true;
}

// ---------------------
// Sound effects
// ---------------------

/** Little-endian reads from a WAV file's bytes. */
inline uint ReadLE32(const byte *p) { return (uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) | ((uint)p[3] << 24); }
inline ushort ReadLE16(const byte *p) { return (ushort)(p[0] | (p[1] << 8)); }

/**
 * (Plus) Converts a PCM WAV file (8 or 16-bit, mono or stereo, any rate) into
 * interleaved stereo 16-bit at AUDIO_FREQUENCY, as SDL_LoadWAV_RW() and
 * SDL_ConvertAudio() did for the decompilation.
 *
 * @return The samples (malloc'ed, @p length Sint16s), or null if it is not such a file.
 */
Sint16 *DecodeWav(const byte *data, int size, size_t *length)
{
    if (size < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
        return nullptr;

    int channels = 0, rate = 0, bits = 0, format = 0;
    const byte *pcm = nullptr;
    uint pcmSize    = 0;
    for (int pos = 12; pos + 8 <= size;) {
        const uint chunkSize = ReadLE32(data + pos + 4);
        const byte *chunk    = data + pos + 8;
        const uint available = (uint)(size - pos - 8);
        if (!memcmp(data + pos, "fmt ", 4) && chunkSize >= 16 && available >= 16) {
            format   = ReadLE16(chunk);
            channels = ReadLE16(chunk + 2);
            rate     = (int)ReadLE32(chunk + 4);
            bits     = ReadLE16(chunk + 14);
        }
        else if (!memcmp(data + pos, "data", 4)) {
            pcm     = chunk;
            pcmSize = chunkSize < available ? chunkSize : available;
        }
        pos += 8 + (int)chunkSize + (chunkSize & 1);
    }
    if (format != 1 || (channels != 1 && channels != 2) || (bits != 8 && bits != 16) || rate <= 0 || !pcm)
        return nullptr;

    const int frameBytes = channels * bits / 8;
    const size_t frames  = pcmSize / frameBytes;
    if (!frames)
        return nullptr;
    auto sampleAt = [&](size_t frame, int channel) -> float {
        const byte *p = pcm + frame * frameBytes + channel * (bits / 8);
        return bits == 8 ? (p[0] - 128) * 256.0f : (float)(short)ReadLE16(p);
    };

    // Linear resampling to AUDIO_FREQUENCY.
    const size_t outFrames = (size_t)((double)frames * AUDIO_FREQUENCY / rate);
    Sint16 *out            = (Sint16 *)malloc((outFrames ? outFrames : 1) * 2 * sizeof(Sint16));
    if (!out)
        return nullptr;
    const double step = (double)rate / AUDIO_FREQUENCY;
    for (size_t f = 0; f < outFrames; ++f) {
        const double source = f * step;
        const size_t a      = (size_t)source;
        const size_t b      = a + 1 < frames ? a + 1 : a;
        const float t       = (float)(source - a);
        for (int c = 0; c < 2; ++c) {
            const int channel = channels == 2 ? c : 0;
            const float value = sampleAt(a, channel) + (sampleAt(b, channel) - sampleAt(a, channel)) * t;
            out[f * 2 + c]    = (Sint16)(value > 32767.0f ? 32767.0f : value < -32768.0f ? -32768.0f : value);
        }
    }
    *length = outFrames * 2;
    return out;
}

void LoadSfx(char *filePath, byte sfxID)
{
    if (!audioEnabled)
        return;

    FileInfo info;
    char fullPath[0x80];

    StrCopy(fullPath, "Data/SoundFX/");
    StrAdd(fullPath, filePath);

    if (LoadFile(fullPath, &info)) {
        byte *sfx = new byte[info.fileSize];
        FileRead(sfx, info.fileSize);
        CloseFile();

        // unencrypt sfx
        if (info.encrypted) {
            for (int i = 0; i < info.fileSize; ++i) sfx[i] ^= 0xFF;
        }

        size_t length  = 0;
        Sint16 *buffer = DecodeWav(sfx, info.fileSize, &length);
        delete[] sfx;
        if (!buffer) {
            PrintLog("Unable to read sfx: %s", info.fileName);
            return;
        }

        std::lock_guard<std::mutex> guard(audioLock);
        if (sfxList[sfxID].loaded)
            free(sfxList[sfxID].buffer);
        StrCopy(sfxList[sfxID].name, filePath);
        sfxList[sfxID].buffer = buffer;
        sfxList[sfxID].length = length;
        sfxList[sfxID].loaded = true;
    }
}

void PlaySfx(int sfx, bool loop)
{
    std::lock_guard<std::mutex> guard(audioLock);
    int sfxChannelID = nextChannelPos++;
    for (int c = 0; c < CHANNEL_COUNT; ++c) {
        if (sfxChannels[c].sfxID == sfx) {
            sfxChannelID = c;
            break;
        }
    }

    ChannelInfo *sfxInfo  = &sfxChannels[sfxChannelID];
    sfxInfo->sfxID        = sfx;
    sfxInfo->samplePtr    = sfxList[sfx].buffer;
    sfxInfo->sampleLength = sfxList[sfx].length;
    sfxInfo->loopSFX      = loop;
    sfxInfo->pan          = 0;
    if (nextChannelPos == CHANNEL_COUNT)
        nextChannelPos = 0;
}

void StopSfx(int sfx)
{
    std::lock_guard<std::mutex> guard(audioLock);
    StopSfxLocked(sfx);
}

void SetSfxAttributes(int sfx, int loopCount, sbyte pan)
{
    std::lock_guard<std::mutex> guard(audioLock);
    int sfxChannel = -1;
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID == sfx || sfxChannels[i].sfxID == -1) {
            sfxChannel = i;
            break;
        }
    }
    if (sfxChannel == -1)
        return; // wasn't found

    // TODO: is this right? should it play an sfx here? without this rings dont play any sfx so I assume it must be?
    ChannelInfo *sfxInfo  = &sfxChannels[sfxChannel];
    sfxInfo->samplePtr    = sfxList[sfx].buffer;
    sfxInfo->sampleLength = sfxList[sfx].length;
    sfxInfo->loopSFX      = loopCount == -1 ? sfxInfo->loopSFX : loopCount;
    sfxInfo->pan          = pan;
    sfxInfo->sfxID        = sfx;
}

void StopAllSfx()
{
    std::lock_guard<std::mutex> guard(audioLock);
    for (int i = 0; i < CHANNEL_COUNT; ++i) sfxChannels[i].sfxID = -1;
}

/** Frees sound effect slot @p i. Under audioLock. */
void FreeSfxLocked(int i)
{
    if (sfxList[i].loaded) {
        StrCopy(sfxList[i].name, "");
        free(sfxList[i].buffer);
        sfxList[i].buffer = nullptr;
        sfxList[i].length = 0;
        sfxList[i].loaded = false;
    }
}

void ReleaseGlobalSfx()
{
    StopAllSfx();
    std::lock_guard<std::mutex> guard(audioLock);
    for (int i = NoGlobalSFX - 1; i >= 0; --i) FreeSfxLocked(i);
    NoGlobalSFX = 0;
}

void ReleaseStageSfx()
{
    std::lock_guard<std::mutex> guard(audioLock);
    // (Plus) A channel may still point at the samples freed here.
    for (int c = 0; c < CHANNEL_COUNT; ++c) {
        if (sfxChannels[c].sfxID >= NoGlobalSFX)
            sfxChannels[c].sfxID = -1;
    }
    for (int i = NoStageSFX + NoGlobalSFX; i >= NoGlobalSFX; --i) FreeSfxLocked(i);
    NoStageSFX = 0;
}

void ReleaseSoundDevice()
{
    StopMusic();
    StopAllSfx();
    ReleaseStageSfx();
    ReleaseGlobalSfx();
    std::lock_guard<std::mutex> guard(audioLock);
    audioEnabled = false;
}
