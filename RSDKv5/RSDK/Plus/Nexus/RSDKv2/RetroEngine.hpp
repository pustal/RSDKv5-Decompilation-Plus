#ifndef RSDKV2_RETROENGINE_H
#define RSDKV2_RETROENGINE_H

// (Plus) RSDKv2, the engine of Sonic Nexus, from the RSDKv2 decompilation
// (https://github.com/RSDKModding/RSDKv2-Decompilation, see LICENSE.md in
// this folder), hosted by the RSDKv5U engine as its "engine version 2"
// (see ../NexusCore.hpp). Changes from the decompilation, all marked (Plus):
//   - no platform layer: no SDL window, events, renderer, audio device,
//     game controllers, settings.ini, log file or mod loader. The host gives
//     the frame's input (Input.cpp), takes the finished frame as RGB565
//     (Drawing.cpp, FlipScreen()) and mixes the sound (Audio.cpp);
//   - files are opened with the host's function (Reader.cpp), so the data
//     file can be in a folder plain fopen() cannot reach (Android's picker);
//   - music is decoded with stb_vorbis (which the host engine links) instead
//     of libvorbisfile, and sound effects are read and resampled without SDL;
//   - everything is in namespace RSDKv2 (NexusCore.cpp), and this header
//     includes no system header: NexusCore.cpp includes them first.

// Setting this to true removes (almost) ALL changes from the original code, the trade off is that a playable game cannot be built, it is advised to
// be set to true only for preservation purposes
#define RETRO_USE_ORIGINAL_CODE (0)

// (Plus) The host has its own mod loader; this core has none.
#define RETRO_USE_MOD_LOADER (0)

// ================
// STANDARD TYPES
// ================
typedef unsigned char byte;
typedef signed char sbyte;
typedef unsigned short ushort;
typedef unsigned int uint;

// (Plus) The SDL types the decompilation's audio code uses.
typedef short Sint16;
typedef int Sint32;

// Platform types (Game manages platform-specific code such as HUD position using this rather than the above)
#define RETRO_STANDARD (0)
#define RETRO_MOBILE   (1)

#define DEFAULT_SCREEN_XSIZE 320

enum RetroStates {
    ENGINE_SYSMENU     = 0,
    ENGINE_MAINGAME    = 1,
    ENGINE_INITSYSMENU = 2,
    ENGINE_EXITGAME    = 3,
};

// General Defines
#define SCREEN_YSIZE   (240)
#define SCREEN_CENTERY (SCREEN_YSIZE / 2)

extern bool engineDebugMode;

// Utils
#include "Math.hpp"
#include "String.hpp"
#include "Reader.hpp"
#include "Animation.hpp"
#include "Audio.hpp"
#include "Input.hpp"
#include "Object.hpp"
#include "Player.hpp"
#include "Palette.hpp"
#include "Drawing.hpp"
#include "Collision.hpp"
#include "Scene.hpp"
#include "Script.hpp"
#include "Sprite.hpp"
#include "Text.hpp"
#include "Video.hpp"
#include "Userdata.hpp"
#include "Debug.hpp"

class RetroEngine
{
public:
    RetroEngine() {}

    bool UseBinFile         = false;
    bool usingDataFileStore = false;
    bool forceFolder        = false;

    char dataFile[0x200]; // (Plus) The data file's full path, from the host.

    bool initialised = false;
    bool GameRunning = false;

    int GameMode    = ENGINE_MAINGAME;
    byte ColourMode = 1; // 16-bit

    int frameSkipSetting = 0;
    int frameSkipTimer   = 0;

    // Ported from RSDKv5
    int startList_Game  = 0xFF; // (Plus) As InitUserdata() set them: no starting stage.
    int startStage_Game = 0xFF;

    bool consoleEnabled  = false;
    bool devMenu         = false;
    int startList        = -1;
    int startStage       = -1;
    int gameSpeed        = 1;
    int fastForwardSpeed = 8;
    bool masterPaused    = false;
    bool frameStep       = false;

    char startSceneFolder[0x10];
    char startSceneID[0x10];

    /** (Plus) Loads the game from dataFile. @return Whether it can run. */
    bool Init();
    /** (Plus) One frame of the game (what Run()'s loop did once), drawing into FrameBuffer. */
    void RunFrame();
    /** (Plus) Frees what Init() allocated. */
    void Release();

    bool LoadGameConfig(const char *Filepath);

    char GameWindowText[0x40];
    char GameDescriptionText[0x100];

    byte *FrameBuffer = nullptr;

    int refreshRate       = 60; // user-picked screen update rate
    int targetRefreshRate = 60; // game logic update rate

    uint frameCount      = 0; // frames since scene load
    int renderFrameIndex = 0;
    int skipFrameIndex   = 0;
};

extern RetroEngine Engine;
#endif // !RSDKV2_RETROENGINE_H
