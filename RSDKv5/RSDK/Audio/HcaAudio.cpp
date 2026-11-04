// =====================
// (Plus) CRI HCA audio (see HcaAudio.hpp)
// =====================
//
// Included by Audio.cpp, after clHCA (dependencies/all/clHCA/clhca.c).

#include "HcaAudio.hpp"

namespace RSDK
{
namespace Hca
{

/** Samples a block holds, per channel (clHCA_stInfo::samplesPerBlock is always this). */
const uint32 kSamplesPerBlock = 1024;
/** Largest channel count decoded (clHCA allows up to 16). */
const uint32 kMaxChannels = 16;
/** A .wav effect's level (LoadSfxToSlot(): 16-bit samples are scaled by 0.75). */
const float kEffectLevel = 0.75f;

/** A clHCA decoder, freed when it goes out of scope. */
struct Decoder {
    clHCA *hca = clHCA_new();
    ~Decoder() { clHCA_delete(hca); }
    Decoder()                           = default;
    Decoder(const Decoder &)            = delete;
    Decoder &operator=(const Decoder &) = delete;
};

/** Reads @p data's header into @p hca and @p info. @return False if it is not a playable HCA file. */
bool32 ReadHeader(clHCA *hca, const uint8 *data, uint32 size, clHCA_stInfo *info)
{
    if (!hca || !IsHca(data, size))
        return false;
    const int32 headerSize = clHCA_isOurFile(data, size);
    if (headerSize <= 0 || (uint32)headerSize > size)
        return false;
    if (clHCA_DecodeHeader(hca, data, (unsigned int)headerSize) < 0 || clHCA_getInfo(hca, info) < 0)
        return false;
    // Only files stored without their key are played (the converter decrypts them).
    if (info->encryptionEnabled || !info->channelCount || info->channelCount > kMaxChannels || !info->samplingRate
        || info->samplesPerBlock != kSamplesPerBlock || !info->blockSize)
        return false;
    return info->headerSize + (uint64)info->blockSize * info->blockCount <= size;
}

bool32 IsHca(const uint8 *data, uint32 size) { return data && size >= 8 && clHCA_isOurFile(data, size) > 0; }

bool32 DecodeEffect(const uint8 *data, uint32 size, std::vector<float> *samples)
{
    Decoder decoder;
    clHCA_stInfo info;
    if (!ReadHeader(decoder.hca, data, size, &info))
        return false;

    // Every block, mixed down to mono, then the encoder's delay skipped.
    std::vector<float> mono;
    mono.reserve((size_t)info.blockCount * kSamplesPerBlock);
    std::vector<float> block(kSamplesPerBlock * info.channelCount);
    std::vector<uint8> scratch(info.blockSize);
    for (uint32 b = 0; b < info.blockCount; ++b) {
        memcpy(scratch.data(), data + info.headerSize + (size_t)b * info.blockSize, info.blockSize);
        if (clHCA_DecodeBlock(decoder.hca, scratch.data(), info.blockSize) < 0)
            return false;
        clHCA_ReadSamples(decoder.hca, block.data());
        for (uint32 s = 0; s < kSamplesPerBlock; ++s) {
            float sum = 0.0f;
            for (uint32 c = 0; c < info.channelCount; ++c) sum += block[s * info.channelCount + c];
            mono.push_back(sum / info.channelCount);
        }
    }
    const size_t first = info.encoderDelay < mono.size() ? info.encoderDelay : mono.size();
    const size_t count = info.sampleCount < mono.size() - first ? info.sampleCount : mono.size() - first;
    if (!count)
        return false;

    // The mixer's rate, by linear interpolation (as Ogg Vorbis effects are converted).
    const double step   = info.samplingRate / (double)AUDIO_FREQUENCY;
    const size_t length = (size_t)(count / step);
    samples->resize(length);
    for (size_t s = 0; s < length; ++s) {
        const double pos   = s * step;
        const size_t index = (size_t)pos;
        const float frac   = (float)(pos - index);
        const float a      = mono[first + index];
        const float b      = index + 1 < count ? mono[first + index + 1] : a;
        (*samples)[s]      = (a + (b - a) * frac) * kEffectLevel;
    }
    return true;
}

// ---------------------
// Stream
// ---------------------

Stream::Stream() : decoder(clHCA_new()) {}

Stream::~Stream() { clHCA_delete(decoder); }

bool32 Stream::Open(std::vector<uint8> &&file, float speed, uint32 startPos)
{
    data = std::move(file);
    clHCA_stInfo info;
    if (!ReadHeader(decoder, data.data(), (uint32)data.size(), &info))
        return false;

    headerSize   = info.headerSize;
    blockSize    = info.blockSize;
    blockCount   = info.blockCount;
    channels     = info.channelCount;
    sampleRate   = info.samplingRate;
    encoderDelay = info.encoderDelay;
    sampleCount  = info.sampleCount;
    if (info.loopEnabled && info.loopStartSample < info.loopEndSample && info.loopEndSample <= info.sampleCount) {
        loopStart = info.loopStartSample;
        loopEnd   = info.loopEndSample;
    }
    else {
        loopStart = 0;
        loopEnd   = sampleCount;
    }

    step = sampleRate * (double)(speed > 0.0f ? speed : 1.0f) / AUDIO_FREQUENCY;
    blockBuffer.resize(kSamplesPerBlock * channels);
    sourcePos = startPos * step;
    if (sourcePos >= sampleCount)
        sourcePos = 0.0;
    return SeekSource((int64)sourcePos);
}

bool32 Stream::DecodeBlock(uint32 block)
{
    if (block >= blockCount)
        return false;
    // clHCA decodes in place; keep the file as it is for the next loop.
    uint8 scratch[0x10000];
    if (blockSize > sizeof(scratch))
        return false;
    memcpy(scratch, data.data() + headerSize + (size_t)block * blockSize, blockSize);
    if (clHCA_DecodeBlock(decoder, scratch, blockSize) < 0)
        memset(blockBuffer.data(), 0, blockBuffer.size() * sizeof(float)); // A damaged block plays as silence.
    else
        clHCA_ReadSamples(decoder, blockBuffer.data());
    return true;
}

bool32 Stream::SeekSource(int64 frame)
{
    if (frame < 0)
        frame = 0;
    const uint32 block = (uint32)((frame + encoderDelay) / kSamplesPerBlock);
    clHCA_DecodeReset(decoder);
    // Each block overlaps the one before: decode that one first, unheard.
    if (block > 0)
        DecodeBlock(block - 1);
    window.clear();
    windowStart = (int64)block * kSamplesPerBlock - encoderDelay;
    nextBlock   = block;
    return block < blockCount;
}

bool32 Stream::Frame(int64 frame, float *left, float *right)
{
    if (frame < windowStart)
        SeekSource(frame);

    while (frame >= windowStart + (int64)(window.size() / 2)) {
        // Keep only the frame before this one, for the interpolation.
        const int64 keepFrom = frame - 1 > windowStart ? frame - 1 : windowStart;
        const size_t drop    = (size_t)(keepFrom - windowStart) * 2;
        if (drop >= window.size()) {
            windowStart += (int64)(window.size() / 2);
            window.clear();
        }
        else if (drop > 0) {
            window.erase(window.begin(), window.begin() + drop);
            windowStart = keepFrom;
        }

        if (!DecodeBlock(nextBlock))
            return false;
        ++nextBlock;
        for (uint32 s = 0; s < kSamplesPerBlock; ++s) {
            const float *in = &blockBuffer[s * channels];
            window.push_back(in[0]);
            window.push_back(channels > 1 ? in[1] : in[0]);
        }
    }

    const size_t index = (size_t)(frame - windowStart) * 2;
    *left              = window[index];
    *right             = window[index + 1];
    return true;
}

int32 Stream::Read(float *out, int32 frames, bool32 loop)
{
    for (int32 f = 0; f < frames; ++f) {
        if (sourcePos >= (loop ? loopEnd : sampleCount)) {
            if (!loop || loopEnd <= loopStart)
                return f;
            sourcePos -= (double)(loopEnd - loopStart);
        }

        const int64 index = (int64)sourcePos;
        const float frac  = (float)(sourcePos - index);
        float l0 = 0.0f, r0 = 0.0f;
        if (!Frame(index, &l0, &r0))
            return f;
        float l1 = l0, r1 = r0;
        if (index + 1 < (loop ? loopEnd : sampleCount))
            Frame(index + 1, &l1, &r1);

        out[f * 2 + 0] = l0 + (l1 - l0) * frac;
        out[f * 2 + 1] = r0 + (r1 - r0) * frac;
        sourcePos += step;
    }
    return frames;
}

uint32 Stream::Position() const { return (uint32)(sourcePos / step); }

} // namespace Hca
} // namespace RSDK
