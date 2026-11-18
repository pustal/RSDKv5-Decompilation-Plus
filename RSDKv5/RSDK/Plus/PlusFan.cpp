// =====================
// Plus: fan games (Sonic Nexus)
// =====================
//
// Included by Plus.cpp, after PlusPacks.cpp. Fan games made with an earlier
// Retro Engine, kept in a "fan" folder next to Sonic Mania's Data.rsdk under
// fixed names, offered by the Plus menu's Games page in a "Fan games" section
// that only shows when one is there:
//   - fan/sonic-nexus.bin: Sonic Nexus (2008) by Christian Whitehead, its
//     Data.bin renamed. It runs on RSDKv2, hosted as "engine version 2"
//     (Nexus/NexusCore.hpp): the main loop calls ProcessFanGame(), the audio
//     device MixFanGameAudio(), and the Plus menu pauses it like any game
//     (Legacy::gameMode switches between ENGINE_MAINGAME and ENGINE_DEVMENU,
//     as for the legacy cores, so the engine's menu handling works as is).
// Sonic Nexus was made for a 320-pixel-wide screen (its title screen and HUD
// are placed for it), so it is drawn at that width, centred, with black bars
// on a wider screen.

namespace RSDK
{
namespace Plus
{

/** Folder (inside the user folder) holding the fan games, and the saves of each in "<folder>/<game>/". */
const char *const kFanFolder = "fan";

/** A fan game the Plus menu knows. */
struct FanGameInfo {
    const char *id;       ///< In Plus.ini's lastGame.
    const char *title;    ///< Shown in the menu.
    const char *fileName; ///< In the "fan" folder.
};

/** The fan games, in menu order. */
const FanGameInfo kFanGames[] = {
    { "nexus", "Sonic Nexus", "sonic-nexus.bin" },
};

/** A fan game found. */
struct FanGame {
    FanGameInfo info{};
    std::string path; ///< Relative to the user folder ("fan/sonic-nexus.bin"), or absolute (AddPackFolder()).
};

/** The fan games found at startup (ScanFanGames()). */
std::vector<FanGame> fanGames;

/** The screen width Sonic Nexus was made for (see the notes above). */
const int32 kFanScreenWidth = 320;

/** Smallest file that can be an RSDKv2 data file: its header and a directory. */
const int64 kMinimumFanDataBytes = 64;

/** True when @p relative (in the user folder, or absolute) is a file that can be an RSDKv2 data file. */
bool LooksLikeFanData(const std::string &relative)
{
    PackFile file(UserPath(relative), "rb");
    if (!file.file)
        return false;
    // An RSDKv2 data file starts with its header's size (32-bit) and its directory count.
    uint32 headerSize = 0;
    uint8 dirCount    = 0;
    const int64 size  = FileSizeOf(file.file);
    return size >= kMinimumFanDataBytes && ReadU32(file.file, &headerSize) && ReadU8(file.file, &dirCount) && headerSize > 5 && headerSize < size
           && dirCount > 0;
}

/**
 * The "fan" folder next to @p packFolder (an AddPackFolder() folder, with its
 * trailing slash): "/usb/SonicWebMania/fan/" for "/usb/SonicWebMania/retro/".
 */
std::string SiblingFanFolder(const std::string &packFolder)
{
    const std::string trimmed = packFolder.substr(0, packFolder.size() - 1);
    const size_t slash        = trimmed.find_last_of('/');
    return (slash == std::string::npos ? std::string() : trimmed.substr(0, slash + 1)) + kFanFolder + "/";
}

/**
 * Finds the fan games: in the user folder's "fan" folder, else, for each
 * folder added with AddPackFolder(), in the "fan" folder next to it (a USB
 * drive's SonicWebMania/fan beside SonicWebMania/retro) or in it.
 */
std::vector<FanGame> ScanFanGames()
{
    std::vector<FanGame> found;
    for (const FanGameInfo &info : kFanGames) {
        std::vector<std::string> candidates = { std::string(kFanFolder) + "/" + info.fileName };
        for (const std::string &folder : extraPackFolders) {
            candidates.push_back(SiblingFanFolder(folder) + info.fileName);
            candidates.push_back(folder + info.fileName);
        }
        for (const std::string &candidate : candidates) {
            if (LooksLikeFanData(candidate)) {
                found.push_back({ info, candidate });
                break;
            }
        }
    }
    return found;
}

/** The fan game @p id when it was found, else null. */
const FanGame *FindFanGame(const std::string &id)
{
    for (const FanGame &game : fanGames) {
        if (id == game.info.id)
            return &game;
    }
    return nullptr;
}

bool IsFanGame(const std::string &id) { return FindFanGame(id) != nullptr; }

/** "fan/<game>/": where a fan game keeps its files (its Achievements.bin), full path with its trailing slash. */
std::string FanSaveFolder(const std::string &id) { return UserPath(std::string(kFanFolder) + "/" + id + "/"); }

// ---------------------
// The hosted core
// ---------------------

const std::string &CurrentGameId(); // Plus.cpp

/** Whether the audio device may mix the core's sound (between StartFanGame() and StopFanGame()). */
std::atomic<bool> fanAudioActive(false);

FILE *OpenFanFile(const char *path, const char *mode) { return OpenUserFile(path, mode); }

void LogFanLine(const char *text) { PrintLog(PRINT_NORMAL, "[Nexus] %s", text); }

/** Stops the hosted core (leaving the game, or at shutdown). */
void StopFanGame()
{
    fanAudioActive = false;
    RSDKv2Host::Release();
}

/** Fills the screen with black (behind the game's centred picture, or when it could not start). */
void ClearFanScreen()
{
    ScreenInfo &screen = screens[0];
    for (int32 y = 0; y < SCREEN_YSIZE; ++y) memset(&screen.frameBuffer[y * screen.pitch], 0, screen.size.x * sizeof(uint16));
}

/** Copies the core's last frame to the engine's screen, centred (both are RGB565). */
void DrawFanFrame()
{
    ScreenInfo &screen       = screens[0];
    const uint16 *source = RSDKv2Host::Frame();
    ClearFanScreen();
    if (!source)
        return;

    const int32 sourceWidth = RSDKv2Host::ScreenWidth();
    const int32 width       = sourceWidth < screen.size.x ? sourceWidth : screen.size.x;
    const int32 height      = RSDKv2Host::kScreenHeight < screen.size.y ? RSDKv2Host::kScreenHeight : screen.size.y;
    const int32 destX       = (screen.size.x - width) / 2;
    const int32 sourceX     = (sourceWidth - width) / 2;
    for (int32 y = 0; y < height; ++y)
        memcpy(&screen.frameBuffer[y * screen.pitch + destX], &source[y * sourceWidth + sourceX], width * sizeof(uint16));
}

/** The buttons player 1 holds, for the core: the d-pad or left stick, A B C (X Y Z too), Start. */
RSDKv2Host::Input ReadFanInput()
{
    const ControllerState &pad = controller[CONT_ANY];
    const AnalogState &stick   = stickL[CONT_ANY];
    RSDKv2Host::Input input;
    input.up    = pad.keyUp.down || stick.keyUp.down;
    input.down  = pad.keyDown.down || stick.keyDown.down;
    input.left  = pad.keyLeft.down || stick.keyLeft.down;
    input.right = pad.keyRight.down || stick.keyRight.down;
    input.a     = pad.keyA.down || pad.keyX.down;
    input.b     = pad.keyB.down || pad.keyY.down;
    input.c     = pad.keyC.down || pad.keyZ.down;
    input.start = pad.keyStart.down;
    return input;
}

} // namespace Plus
} // namespace RSDK

bool32 RSDK::Plus::StartFanGame()
{
    StopFanGame();
    Legacy::gameMode = Legacy::ENGINE_MAINGAME;

    const FanGame *game = FindFanGame(CurrentGameId());
    RSDKv2Host::Host host;
    host.openFile = OpenFanFile;
    host.log      = LogFanLine;
    if (!game || !RSDKv2Host::Init(host, UserPath(game->path).c_str(), kFanScreenWidth)) {
        PrintLog(PRINT_NORMAL, "[Plus] Could not start %s", CurrentGameId().c_str());
        ClearFanScreen();
        OpenMenu();
        Notify("Could not start that game: is its file complete?");
        return false;
    }

    sprintf_s(gameVerInfo.gameTitle, sizeof(gameVerInfo.gameTitle), "%s", game->info.title);
    sprintf_s(gameVerInfo.version, sizeof(gameVerInfo.version), "%s", "RSDKv2 core");
    fanAudioActive = true;
    PrintLog(PRINT_NORMAL, "[Plus] Started %s (%s)", game->info.title, RSDKv2Host::Title());
    return true;
}

void RSDK::Plus::ProcessFanGame()
{
    switch (Legacy::gameMode) {
        case Legacy::ENGINE_DEVMENU:
            // The Plus menu, over the frozen picture (as the legacy cores do).
            ProcessInput();
            currentScreen = &screens[0];
            if (devMenu.state)
                devMenu.state();
            break;

        case Legacy::ENGINE_MAINGAME:
            ProcessInput();
            currentScreen = &screens[0];
            if (!RSDKv2Host::Running())
                break; // It could not start: the Plus menu says so.
            RSDKv2Host::SetVolumes((int32)(engine.streamVolume * 100), (int32)(engine.soundFXVolume * 100));
            RSDKv2Host::RunFrame(ReadFanInput());
            if (!RSDKv2Host::Running()) {
                // The game quit (its menu's Quit), as a legacy game's ENGINE_EXITGAME does.
                StopFanGame();
                SKU::ExitGame();
                break;
            }
            DrawFanFrame();
            break;

        default: break;
    }
}

void RSDK::Plus::MixFanGameAudio(void *stream, int32 length)
{
    // Not while the Plus menu is open: the game is paused, its sound too.
    if (engine.version != 2 || !fanAudioActive || Legacy::gameMode != Legacy::ENGINE_MAINGAME)
        return;

    SAMPLE_FORMAT *out = (SAMPLE_FORMAT *)stream;
    int16 buffer[MIX_BUFFER_SIZE];
    for (int32 done = 0; done < length;) {
        const int32 count = length - done < MIX_BUFFER_SIZE ? length - done : MIX_BUFFER_SIZE;
        RSDKv2Host::MixAudio(buffer, (size_t)count);
        for (int32 i = 0; i < count; ++i) {
#if RETRO_PLATFORM == RETRO_WII // The Wii mixes 16-bit integers.
            out[done + i] = (SAMPLE_FORMAT)CLAMP((int32)out[done + i] + buffer[i], -0x7FFF, 0x7FFF);
#else
            out[done + i] += buffer[i] * (1.0f / 32768.0f);
#endif
        }
        done += count;
    }
}
