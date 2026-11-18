// =====================
// Plus: Sonic Nexus's engine (RSDKv2), hosted (see NexusCore.hpp)
// =====================
//
// Included by RetroEngine.cpp before any RSDKv5 header, so this is the same
// translation unit as the engine (no project file lists it, as with the
// Plus module and the legacy cores) while RSDKv2's macros, which share many
// names with RSDKv5's (SCREEN_YSIZE, SFX_COUNT, CHANNEL_COUNT...), are all
// #undef'd at the end (NexusUndef.hpp) before RSDKv5's are defined.
//
// RSDKv2's code is the decompilation's, in namespace RSDKv2. Its .cpp files
// include "RetroEngine.hpp" and a few system headers themselves; those are
// all included here first, outside the namespace, so their include guards
// make the later includes inside it do nothing.

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <math.h>
#include <mutex>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// stb_vorbis's declarations only: RSDKv5's Audio.cpp compiles it (with these
// options), and this core decodes its music with it too.
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_INTEGER_CONVERSION
#include "stb_vorbis/stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY
#undef STB_VORBIS_NO_PUSHDATA_API
#undef STB_VORBIS_NO_STDIO
#undef STB_VORBIS_NO_INTEGER_CONVERSION

#include "NexusCore.hpp"

namespace RSDKv2
{
#include "RSDKv2/RetroEngine.hpp"

#include "RSDKv2/Animation.cpp"
#include "RSDKv2/Audio.cpp"
#include "RSDKv2/Collision.cpp"
#include "RSDKv2/Debug.cpp"
#include "RSDKv2/Drawing.cpp"
#include "RSDKv2/Input.cpp"
#include "RSDKv2/Math.cpp"
#include "RSDKv2/Object.cpp"
#include "RSDKv2/Palette.cpp"
#include "RSDKv2/Player.cpp"
#include "RSDKv2/Reader.cpp"
#include "RSDKv2/RetroEngine.cpp"
#include "RSDKv2/Scene.cpp"
#include "RSDKv2/Script.cpp"
#include "RSDKv2/Sprite.cpp"
#include "RSDKv2/String.cpp"
#include "RSDKv2/Text.cpp"
#include "RSDKv2/Userdata.cpp"
#include "RSDKv2/Video.cpp"

/** The host's functions (Init()). */
RSDKv2Host::Host host;

FileIO *HostOpenFile(const char *path, const char *mode) { return host.openFile ? host.openFile(path, mode) : nullptr; }

void HostLog(const char *text)
{
    if (host.log)
        host.log(text);
}

/**
 * RSDKv2's largest tables (7.5 MB: graphics, layouts, scripts, tiles, stage
 * lists), which the decompilation keeps as static arrays, are allocated
 * while the game runs instead: as static arrays they would take that memory
 * from every game the engine plays, and the Wii's executable must fit in its
 * 24 MB of MEM1. Zeroed, as static arrays start.
 *
 * @return False when there is not enough memory (nothing is kept then).
 */
bool AllocateLargeData()
{
    GraphicData  = static_cast<byte *>(calloc(GFXDATA_MAX, sizeof(byte)));
    stageList    = static_cast<SceneInfo(*)[0x100]>(calloc(STAGELIST_MAX, sizeof(*stageList)));
    StageLayouts = static_cast<TileLayer *>(calloc(LAYER_COUNT, sizeof(TileLayer)));
    StageTiles   = static_cast<Tiles128x128 *>(calloc(1, sizeof(Tiles128x128)));
    TileGfx      = static_cast<byte *>(calloc(TILESET_SIZE, sizeof(byte)));
    ScriptData   = static_cast<int *>(calloc(SCRIPTDATA_COUNT, sizeof(int)));
    return GraphicData && stageList && StageLayouts && StageTiles && TileGfx && ScriptData;
}

/** Frees what AllocateLargeData() allocated. */
void FreeLargeData()
{
    free(GraphicData);
    free(stageList);
    free(StageLayouts);
    free(StageTiles);
    free(TileGfx);
    free(ScriptData);
    GraphicData  = nullptr;
    stageList    = nullptr;
    StageLayouts = nullptr;
    StageTiles   = nullptr;
    TileGfx      = nullptr;
    ScriptData   = nullptr;
}

} // namespace RSDKv2

bool RSDKv2Host::Init(const Host &host, const char *dataFile, int32_t screenWidth)
{
    using namespace RSDKv2;
    Release();
    RSDKv2::host = host;

    // A fresh engine each time: the game may have been played before in this session.
    Engine = RetroEngine();
    snprintf(Engine.dataFile, sizeof(Engine.dataFile), "%s", dataFile);
    SetScreenSize(screenWidth, screenWidth);
    if (!AllocateLargeData()) {
        HostLog("[Nexus] Not enough memory to start the game");
        Release();
        return false;
    }
    if (!Engine.Init() || !Engine.UseBinFile) {
        HostLog("[Nexus] Could not start the game from its data file");
        Release();
        return false;
    }
    return true;
}

bool RSDKv2Host::Running() { return RSDKv2::Engine.GameRunning; }

void RSDKv2Host::RunFrame(const Input &input)
{
    using namespace RSDKv2;
    if (!Engine.GameRunning)
        return;

    bool held[INPUT_ANY] = {};
    held[INPUT_UP]       = input.up;
    held[INPUT_DOWN]     = input.down;
    held[INPUT_LEFT]     = input.left;
    held[INPUT_RIGHT]    = input.right;
    held[INPUT_BUTTONA]  = input.a;
    held[INPUT_BUTTONB]  = input.b;
    held[INPUT_BUTTONC]  = input.c;
    held[INPUT_START]    = input.start;
    SetHostInput(held);

    Engine.RunFrame();
}

int32_t RSDKv2Host::ScreenWidth() { return RSDKv2::SCREEN_XSIZE; }

const uint16_t *RSDKv2Host::Frame() { return RSDKv2::OutputBuffer; }

void RSDKv2Host::MixAudio(int16_t *output, size_t samples)
{
    static_assert(sizeof(RSDKv2::Sint16) == sizeof(int16_t), "RSDKv2's samples are 16-bit");
    RSDKv2::MixAudio(output, samples);
}

void RSDKv2Host::SetVolumes(int32_t music, int32_t soundEffects)
{
    auto clamp       = [](int32_t value) { return value < 0 ? 0 : value > MAX_VOLUME ? MAX_VOLUME : value; };
    RSDKv2::bgmVolume = clamp(music);
    RSDKv2::sfxVolume = clamp(soundEffects);
}

const char *RSDKv2Host::Title() { return RSDKv2::Engine.initialised ? RSDKv2::Engine.GameWindowText : ""; }

void RSDKv2Host::Release()
{
    using namespace RSDKv2;
    if (Engine.initialised || Engine.FrameBuffer)
        Engine.Release();
    CloseFile();
    FreeLargeData();
}

#include "NexusUndef.hpp"
