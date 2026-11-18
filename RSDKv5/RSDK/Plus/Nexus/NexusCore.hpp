#ifndef PLUS_NEXUS_CORE_H
#define PLUS_NEXUS_CORE_H

/**
 * @file NexusCore.hpp
 * @brief (Plus) Sonic Nexus's engine, RSDKv2, as a core the RSDKv5U engine
 *        hosts (its "engine version 2", see Plus.hpp's "Fan games").
 *
 * The core is the RSDKv2 decompilation (RSDKv2/, with its licence) without
 * its platform layer, compiled in its own namespace by NexusCore.cpp, which
 * RetroEngine.cpp includes before anything of RSDKv5 so the two engines'
 * macros never meet (NexusCore.cpp #undefs RSDKv2's afterwards). This header
 * is all the host sees: plain types, no RSDKv2 header.
 *
 * Threads: everything runs on the engine's thread, except MixAudio(), which
 * the audio device's thread calls.
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace RSDKv2Host
{

/** What the core needs from the host. */
struct Host {
    /** Opens a file like fopen() (the data file, with its full path). */
    FILE *(*openFile)(const char *path, const char *mode) = nullptr;
    /** Writes a line to the log (no trailing newline). */
    void (*log)(const char *text) = nullptr;
};

/** The buttons the game reads (RSDKv2's InputButtons), held or not this frame. */
struct Input {
    bool up    = false;
    bool down  = false;
    bool left  = false;
    bool right = false;
    bool a     = false;
    bool b     = false;
    bool c     = false;
    bool start = false;
};

/** The height of the core's screen, in pixels (RSDKv2's SCREEN_YSIZE). */
const int32_t kScreenHeight = 240;

/**
 * Starts the game in @p dataFile (Sonic Nexus's Data.bin, a full path)
 * on a screen @p screenWidth pixels wide (RSDKv2 draws any width; Sonic Nexus
 * was made for 320).
 *
 * @return False if the data file could not be read (the core is then released).
 */
bool Init(const Host &host, const char *dataFile, int32_t screenWidth);

/** True between Init() and Release(), while the game has not quit. */
bool Running();

/** Runs one frame of the game with @p input, drawing it. */
void RunFrame(const Input &input);

/** The screen's width (Init()'s @p screenWidth). */
int32_t ScreenWidth();

/**
 * The last frame drawn: ScreenWidth() x kScreenHeight pixels, RGB565, rows
 * one after the other. Null before Init().
 */
const uint16_t *Frame();

/**
 * Fills @p output with @p samples 16-bit samples (interleaved stereo at
 * 44100 Hz) of the game's music and sounds. Silence before Init() or when
 * nothing plays. Called from the audio thread.
 */
void MixAudio(int16_t *output, size_t samples);

/** The game's music and sound effect volumes, 0..100 (the host's settings). */
void SetVolumes(int32_t music, int32_t soundEffects);

/** The game's title, from its GameConfig ("" before Init()). */
const char *Title();

/** Stops the game and frees what it holds. Safe to call when not running. */
void Release();

} // namespace RSDKv2Host

#endif // PLUS_NEXUS_CORE_H
