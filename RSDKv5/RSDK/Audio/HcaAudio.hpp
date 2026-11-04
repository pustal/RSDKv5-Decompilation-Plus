#ifndef HCA_AUDIO_H
#define HCA_AUDIO_H

/**
 * @file HcaAudio.hpp
 * @brief (Plus) CRI HCA audio, as Sonic Origins stores its music and sound
 *        effects, for the engine's mixer.
 *
 * The Plus module converts the player's Sonic Origins audio into a data pack
 * of HCA files stored without their encryption, under the names the retro
 * games ask for ("Data/Music/GreenHill.ogg", "Data/SoundFX/Global/Jump.wav";
 * see Plus/PlusOrigins.cpp). The engine tells an HCA file from an Ogg Vorbis
 * or WAV one by its header, so nothing else changes for the games:
 *   - sound effects are decoded when loaded, into the mono float samples a
 *     .wav effect becomes (LoadSfxToSlot());
 *   - music is decoded while it plays (LoadStream(), UpdateStreamBuffer()),
 *     converted from the file's rate (48000 Hz) to the mixer's, and loops
 *     where the file says (Sonic Origins' own loop points: a retro script's
 *     loop point is for the earlier release's track, not this one).
 *
 * Decoding is clHCA's (dependencies/all/clHCA, from vgmstream), built in
 * Audio.cpp's translation unit.
 */

#include <vector>

struct clHCA;

namespace RSDK
{
namespace Hca
{

/** True when @p data starts with an HCA header (masked or not). */
bool32 IsHca(const uint8 *data, uint32 size);

/**
 * Decodes a whole HCA file into mono float samples at AUDIO_FREQUENCY, at the
 * level of a 16-bit .wav effect (the channels are averaged).
 *
 * @param samples Receives the samples.
 * @return False if the file cannot be decoded (encrypted, damaged).
 */
bool32 DecodeEffect(const uint8 *data, uint32 size, std::vector<float> *samples);

/**
 * A music track being played: decodes the blocks it needs as the mixer asks
 * for more, converting the rate (and the speed) as it goes.
 */
class Stream
{
public:
    Stream();
    ~Stream();
    Stream(const Stream &)            = delete;
    Stream &operator=(const Stream &) = delete;

    /**
     * Takes the file (moved in) and prepares to play it.
     *
     * @param speed    1 for the track as it is; 1.2 for Sonic Origins' speed-shoes versions.
     * @param startPos Where to start, in samples at AUDIO_FREQUENCY of the track as played.
     * @return False if the file cannot be played.
     */
    bool32 Open(std::vector<uint8> &&file, float speed, uint32 startPos);

    /**
     * Writes @p frames stereo frames (interleaved, AUDIO_FREQUENCY) to @p out.
     *
     * @param loop Loop at the end (to the file's loop start, or its beginning
     *             when it has none) rather than stop.
     * @return The frames written: fewer than asked once a track that does not
     *         loop has ended.
     */
    int32 Read(float *out, int32 frames, bool32 loop);

    /** The position played so far, in samples at AUDIO_FREQUENCY of the track as played. */
    uint32 Position() const;

private:
    /** Decodes block @p block into the window (after the frames already there). */
    bool32 DecodeBlock(uint32 block);
    /** Starts the window at source frame @p frame (decoding from its block). */
    bool32 SeekSource(int64 frame);
    /** The stereo frame at source frame @p frame (decoding ahead as needed). @return False past the end. */
    bool32 Frame(int64 frame, float *left, float *right);

    clHCA *decoder = nullptr;
    std::vector<uint8> data;
    uint32 headerSize   = 0;
    uint32 blockSize    = 0;
    uint32 blockCount   = 0;
    uint32 channels     = 0;
    uint32 sampleRate   = 0;
    uint32 encoderDelay = 0;
    int64 sampleCount   = 0; ///< Playable source frames.
    int64 loopStart     = 0; ///< Source frame the loop goes back to.
    int64 loopEnd       = 0; ///< Source frame the loop ends at (exclusive).

    double step      = 1.0; ///< Source frames per output frame.
    double sourcePos = 0.0; ///< Source frame being played (fractional).

    std::vector<float> window; ///< Decoded stereo frames, interleaved.
    int64 windowStart    = 0;  ///< Source frame of window[0].
    uint32 nextBlock     = 0;  ///< Block to decode next into the window.
    std::vector<float> blockBuffer;
};

} // namespace Hca
} // namespace RSDK

#endif // HCA_AUDIO_H
