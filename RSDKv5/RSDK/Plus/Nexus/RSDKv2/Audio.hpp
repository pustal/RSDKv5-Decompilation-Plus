#ifndef RSDKV2_AUDIO_H
#define RSDKV2_AUDIO_H

// (Plus) The decompilation's audio without SDL or libvorbisfile (Audio.cpp):
// the same channels and mixing, at the host's rate (44100 Hz, stereo, 16-bit),
// pulled by the host's audio thread through MixAudio().

#define TRACK_COUNT   (0x10)
#define SFX_COUNT     (0x100)
#define CHANNEL_COUNT (0x4)
#define SFXDATA_COUNT (0x400000)

#define MAX_VOLUME (100)

/** (Plus) The rate the host mixes at, and so the rate everything is converted to. */
#define AUDIO_FREQUENCY (44100)
#define AUDIO_CHANNELS  (2)

struct TrackInfo {
    char fileName[0x40];
    bool trackLoop;
};

/** (Plus) Frames decoded at a time. */
#define MUSIC_CHUNK_FRAMES (0x100)

/** (Plus) The music track playing: an Ogg Vorbis file in memory, decoded as it plays. */
struct MusicPlaybackInfo {
    stb_vorbis *vorbis; ///< Null when nothing is loaded.
    FileInfo fileInfo;  ///< Holds the file's bytes (LoadFile2()), which stb_vorbis reads.
    int sourceRate;     ///< The file's sample rate.
    int sourceChannels; ///< 1 or 2.
    // Resampling from sourceRate to AUDIO_FREQUENCY (linear): the two source
    // frames around the output position, and the position between them.
    float frameA[2];
    float frameB[2];
    double position; ///< 0..1 between frameA and frameB.
    float chunk[MUSIC_CHUNK_FRAMES * 2]; ///< Decoded, not yet used (interleaved, sourceChannels wide).
    int chunkFrames;                     ///< Frames in chunk.
    int chunkPos;                        ///< The next frame of chunk to use.
    bool primed;     ///< frameA and frameB hold the first two frames.
    bool trackLoop;
    bool loaded;
};

struct SFXInfo {
    char name[0x40];
    Sint16 *buffer; ///< Interleaved stereo at AUDIO_FREQUENCY (malloc'ed).
    size_t length;  ///< In Sint16s.
    bool loaded;
};

struct ChannelInfo {
    size_t sampleLength;
    Sint16 *samplePtr;
    int sfxID;
    byte loopSFX;
    sbyte pan;
};

enum MusicStatuses {
    MUSIC_STOPPED = 0,
    MUSIC_PLAYING = 1,
    MUSIC_PAUSED  = 2,
    MUSIC_LOADING = 3,
    MUSIC_READY   = 4,
};

extern int NoGlobalSFX;
extern int NoStageSFX;

extern int MusicVolume;
extern int CurrentMusicTrack;
extern int sfxVolume;
extern int bgmVolume;
extern bool audioEnabled;

extern int nextChannelPos;
extern bool musicEnabled;
extern int musicStatus;
extern TrackInfo musicTracks[TRACK_COUNT];
extern SFXInfo sfxList[SFX_COUNT];

extern ChannelInfo sfxChannels[CHANNEL_COUNT];

extern MusicPlaybackInfo musInfo;

/**
 * (Plus) Guards the channels and the music against the host's audio thread.
 * Not recursive: the functions below take it themselves, briefly.
 */
extern std::mutex audioLock;

int InitSoundDevice();
void LoadGlobalSfx();

/**
 * (Plus) Mixes @p samples Sint16s (interleaved stereo, AUDIO_FREQUENCY) of
 * music and sound effects into @p output, replacing what it held. Called by
 * the host's audio thread; returns silence while paused (PauseSound()).
 */
void MixAudio(Sint16 *output, size_t samples);

void LoadMusic(void *userdata);
void SetMusicTrack(char *filePath, byte trackID, bool loop);
bool PlayMusic(int track);
void StopMusic();

void LoadSfx(char *filePath, byte sfxID);
void PlaySfx(int sfx, bool loop);
void StopSfx(int sfx);
void SetSfxAttributes(int sfx, int loopCount, sbyte pan);

inline void SetMusicVolume(int volume)
{
    if (volume < 0)
        volume = 0;
    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;
    MusicVolume = volume;
}

inline void PauseSound()
{
    if (musicStatus == MUSIC_PLAYING)
        musicStatus = MUSIC_PAUSED;
}

inline void ResumeSound()
{
    if (musicStatus == MUSIC_PAUSED)
        musicStatus = MUSIC_PLAYING;
}

void StopAllSfx();
void ReleaseGlobalSfx();
void ReleaseStageSfx();
void ReleaseSoundDevice();

#endif // !RSDKV2_AUDIO_H
