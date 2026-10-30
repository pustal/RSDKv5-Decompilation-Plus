// =====================
// Plus: settings, startup and switching games (see Plus.hpp)
// =====================
//
// Included by RetroEngine.cpp, so the Plus module is one translation unit
// built with the engine on every platform without touching the project files
// (the legacy cores are included the same way). The other parts are
// included below, in dependency order.
//
// Settings are kept in Plus.ini next to Settings.ini:
//   lastGame=sonic1        the game to start with ("mania" or a retro game)
//   maniaAudio=1           Sonic Mania's audio fills the retro games' gaps
//   maniaSimilarMusic=0    ...including similar-feel music for other zones
//   coins=0                Sonic Origins' coins, shared by the retro games

#include <filesystem>
#include <fstream>

#include "PlusPacks.cpp"

namespace RSDK
{
namespace Plus
{

/** Plus.ini's settings. */
struct Config {
    std::string lastGame = kManiaId;
    bool maniaAudio      = true;
    bool similarMusic    = false;
    int32 coins          = 0;
};

/** Everything Plus keeps for the session. */
struct PlusState {
    Config config;
    std::vector<RetroGame> games;           ///< Found at startup.
    std::string current = kManiaId;         ///< The game running now.
    std::string pendingSwitch;              ///< Chosen in the menu, applied outside it.
    std::string maniaDataFile = "Data.rsdk"; ///< Settings.ini Game:dataFile.
    bool maniaAvailable       = false;      ///< Sonic Mania's pack is there.
    bool32 useBuffer          = false;      ///< LoadDataPack's buffering, as Settings.ini chose it.
    int32 maniaReleaseID      = 0;          ///< engine.gameReleaseID as Settings.ini set it.
    std::vector<AchievementInfo> maniaAchievements; ///< Sonic Mania's list, kept while a retro game runs.
    bool maniaAchievementsKept = false;
    char otherUserFileDir[0x100] = {}; ///< The mod loader's save folder, while a retro game's is in use.

    std::string notice;     ///< Shown over the game.
    int32 noticeFrames = 0; ///< Frames it still shows.
};

PlusState plus;
/** Frames since start (the menu uses it to not reopen on the press that closed it). */
uint32 plusFrame = 0;

const char *const kConfigName = "Plus.ini";
/** How long a notice shows, in frames. */
const int32 kNoticeFrames = 4 * 60;

const std::string &CurrentGameId() { return plus.current; }
bool ManiaAvailable() { return plus.maniaAvailable; }
const std::vector<RetroGame> &RetroGames() { return plus.games; }
bool ManiaAudioEnabled() { return plus.config.maniaAudio; }
bool SimilarMusicEnabled() { return plus.config.maniaAudio && plus.config.similarMusic; }
int32 Coins() { return plus.config.coins; }

/** The retro game @p id with its packs, or null for Sonic Mania or an unknown id. */
const RetroGame *FindRetroGame(const std::string &id)
{
    for (const RetroGame &game : plus.games) {
        if (id == game.info.id)
            return &game;
    }
    return nullptr;
}

bool IsRetroGame(const std::string &id) { return FindRetroGame(id) != nullptr; }

/** Interprets the usual spellings of an ini boolean. */
bool ParseBool(const std::string &value) { return value == "1" || value == "true" || value == "y" || value == "yes"; }

void LoadConfig()
{
    std::ifstream in(UserPath(kConfigName));
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const size_t equals = line.find('=');
        if (line.empty() || line[0] == ';' || line[0] == '#' || equals == std::string::npos)
            continue;
        const std::string key   = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);
        if (key == "lastGame")
            plus.config.lastGame = value;
        else if (key == "maniaAudio")
            plus.config.maniaAudio = ParseBool(value);
        else if (key == "maniaSimilarMusic")
            plus.config.similarMusic = ParseBool(value);
        else if (key == "coins")
            plus.config.coins = atoi(value.c_str());
    }
}

void SaveConfig()
{
    std::ofstream out(UserPath(kConfigName), std::ios::trunc);
    out << "; Plus settings (see RSDK/Plus/Plus.cpp)\n";
    out << "lastGame=" << plus.config.lastGame << "\n";
    out << "maniaAudio=" << (plus.config.maniaAudio ? 1 : 0) << "\n";
    out << "maniaSimilarMusic=" << (plus.config.similarMusic ? 1 : 0) << "\n";
    out << "coins=" << plus.config.coins << "\n";
    if (!out)
        PrintLog(PRINT_NORMAL, "[Plus] Could not write %s", kConfigName);
}

void SetCoins(int32 coins)
{
    plus.config.coins = coins;
    SaveConfig();
}

void SetManiaAudioEnabled(bool enabled)
{
    plus.config.maniaAudio = enabled;
    SaveConfig();
}

void SetSimilarMusicEnabled(bool enabled)
{
    plus.config.similarMusic = enabled;
    SaveConfig();
}

/** Asks for a switch to @p id, applied once the menu has closed. */
void RequestSwitch(const std::string &id) { plus.pendingSwitch = id; }

/** Points the save folder at the running retro game's (defined below). */
void EnsureSaveFolder();

} // namespace Plus
} // namespace RSDK

#include "PlusAudio.cpp"
#include "PlusRetro.cpp"
#include "PlusMenu.cpp"

namespace RSDK
{
namespace Plus
{

/** "retro/<game>/": where a retro game keeps its saves, full path with its trailing slash. */
std::string RetroSaveFolder(const std::string &id) { return UserPath(std::string(kRetroFolder) + "/" + id + "/"); }

/**
 * Points the save folder (the mod loader's save redirection) at the running
 * retro game's own folder. The mod loader resets it when it loads mods, so
 * this is checked again before saves are read (OnLegacyStageLoad, Update).
 */
void EnsureSaveFolder()
{
    if (!IsRetroGame(plus.current))
        return;
    const std::string folder = RetroSaveFolder(plus.current);
    if (folder == customUserFileDir)
        return;
    // Whatever was there (a mod's folder, or none) comes back with Sonic Mania.
    if (strncmp(customUserFileDir, UserPath(kRetroFolder).c_str(), UserPath(kRetroFolder).size()) != 0)
        strcpy(plus.otherUserFileDir, customUserFileDir);
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    strcpy(customUserFileDir, folder.c_str());
}

/** Loads the packs of game @p id. @return False if its main pack could not be loaded. */
bool LoadGamePacks(const std::string &id)
{
    UnloadDataPacks();
    if (!IsRetroGame(id)) {
        engine.gameReleaseID = plus.maniaReleaseID;
        return LoadDataPack(plus.maniaDataFile.c_str(), 0, plus.useBuffer);
    }

    // Origins first: the engine takes each file from the first pack that has it.
    const RetroGame &game = *FindRetroGame(id);
    const bool origins    = game.origins.state == PACK_READY;
    if (!LoadDataPack((origins ? game.origins : game.classic).path.c_str(), 0, plus.useBuffer))
        return false;
    if (origins && game.HasSoundPack() && !LoadDataPack(game.classic.path.c_str(), 0, plus.useBuffer))
        PrintLog(PRINT_NORMAL, "[Plus] Could not load %s", game.classic.path.c_str());
    if (plus.config.maniaAudio && plus.maniaAvailable && !LoadAudioDataPack(plus.maniaDataFile.c_str()))
        PrintLog(PRINT_NORMAL, "[Plus] Could not load Sonic Mania's pack for audio");
    engine.gameReleaseID = origins ? 1 : 0; // Game:gameType: 1 Sonic Origins, 0 the earlier releases.
    return true;
}

/** Before leaving the running game: saves its achievements and keeps Sonic Mania's list. */
void LeaveGame()
{
    SKU::SaveUserData(); // Achievements.bin of the game being left, in its save folder.
    if (!IsRetroGame(plus.current)) {
        plus.maniaAchievements     = achievementList;
        plus.maniaAchievementsKept = true;
    }
}

/** After switching to game @p id: its save folder and achievement list. */
void EnterGame(const std::string &id)
{
    plus.current = id;
    ResetRetroSession();
    if (IsRetroGame(id)) {
        EnsureSaveFolder(); // Its achievements are installed when its first stage loads.
        return;
    }
    strcpy(customUserFileDir, plus.otherUserFileDir);
    if (!plus.maniaAchievementsKept)
        return;
    // Sonic Mania's list, unlocked as its Achievements.bin says (saved when it was left).
    achievementList = plus.maniaAchievements;
    int32 saved[kAchievementSlots];
    memset(saved, 0, sizeof(saved));
    if (SKU::LoadUserFile("Achievements.bin", saved, sizeof(saved))) {
        for (int32 i = 0; i < (int32)achievementList.size() && i < kAchievementSlots; ++i) achievementList[i].achieved = saved[i] != 0;
    }
}

/** The game to start with: the last one played if it can be, else Sonic Mania, else the first retro game. */
std::string StartupGame(bool32 maniaLoaded)
{
    const RetroGame *last = FindRetroGame(plus.config.lastGame);
    if (last && last->Playable())
        return last->info.id;
    if (maniaLoaded)
        return kManiaId;
    for (const RetroGame &game : plus.games) {
        if (game.Playable())
            return game.info.id;
    }
    return kManiaId;
}

/** True when @p relative (in the user folder) is a Sonic Mania (RSDKv5) pack. */
bool IsManiaPack(const std::string &relative)
{
    PackFile file(UserPath(relative), "rb");
    uint8 header[6] = {};
    return file.file && fread(header, 1, sizeof(header), file.file) == sizeof(header) && memcmp(header, "RSDKv5", 6) == 0;
}

} // namespace Plus
} // namespace RSDK

bool32 RSDK::Plus::OnStartupPacks(const char *maniaDataFile, bool32 maniaLoaded, bool32 useBuffer)
{
    plus.maniaDataFile  = maniaDataFile;
    plus.useBuffer      = useBuffer;
    plus.maniaReleaseID = engine.gameReleaseID;
    plus.maniaAvailable = maniaLoaded && IsManiaPack(plus.maniaDataFile);
    // Sonic Mania's achievements are registered before the packs load: keep
    // them, in case the session starts in a retro game and comes back.
    plus.maniaAchievements     = achievementList;
    plus.maniaAchievementsKept = true;
    LoadConfig();
    plus.games = ScanRetroGames();

    const std::string game = StartupGame(maniaLoaded);
    if (!IsRetroGame(game))
        return false;
    if (!LoadGamePacks(game)) {
        PrintLog(PRINT_NORMAL, "[Plus] Could not load %s's packs", game.c_str());
        if (maniaLoaded)
            LoadGamePacks(kManiaId);
        return false;
    }
    PrintLog(PRINT_NORMAL, "[Plus] Starting %s", game.c_str());
    EnterGame(game);
    return true;
}

bool32 RSDK::Plus::SwitchPending() { return !plus.pendingSwitch.empty(); }

void RSDK::Plus::ApplyPendingSwitch()
{
    const std::string target = plus.pendingSwitch;
    plus.pendingSwitch.clear();
    const std::string previous = plus.current;

    LeaveGame();
    for (int32 c = 0; c < CHANNEL_COUNT; ++c) StopChannel(c);
    if (!LoadGamePacks(target)) {
        PrintLog(PRINT_NORMAL, "[Plus] Could not load %s's packs", target.c_str());
        LoadGamePacks(previous);
        Notify("Could not load that game's packs");
        return;
    }
    PrintLog(PRINT_NORMAL, "[Plus] Switching to %s", target.c_str());

    // As the main loop does when mods change which game is loaded.
#if RETRO_REV02
    forceHardReset = true;
#endif
    engine.version = 0;
    DetectEngineVersion();
    if (!engine.version)
        engine.version = 5;

    sceneInfo.listData     = NULL;
    sceneInfo.listCategory = NULL;
    globalVarsPtr          = NULL;
    globalVarsInitCB       = NULL;

    ClearGlobalSfx();
    ClearStageSfx();
    dataStorage[DATASET_STG].entryCount  = 0;
    dataStorage[DATASET_STG].usedStorage = 0;
    dataStorage[DATASET_SFX].entryCount  = 0;
    dataStorage[DATASET_SFX].usedStorage = 0;
    for (int32 o = 0; o < objectClassCount; ++o) {
        if (objectClassList[o].staticVars && *objectClassList[o].staticVars)
            (*objectClassList[o].staticVars) = NULL;
    }

    // Controllers as at startup: each player slot takes the next device that
    // is used. Sonic Mania assigns them itself and may have left player 1
    // without one, which the legacy games' scripts would then never hear.
    for (int32 i = 0; i < PLAYER_COUNT; ++i) inputSlots[i] = INPUT_AUTOASSIGN;

    EnterGame(target);
    InitEngine();
    if (engine.version == 5)
        sceneInfo.state = ENGINESTATE_LOAD;
    RenderDevice::SetWindowTitle();

    plus.config.lastGame = target;
    SaveConfig();
}

void RSDK::Plus::Update()
{
    if (plus.noticeFrames > 0)
        --plus.noticeFrames;
    EnsureSaveFolder();

    // Select opens the Plus menu (unless the developer menu's controller
    // debugging already uses it), not on the press that just closed it.
    const bool inMenu = engine.version == 5 ? sceneInfo.state == ENGINESTATE_DEVMENU : Legacy::gameMode == Legacy::ENGINE_DEVMENU;
    if (!inMenu && controller[CONT_ANY].keySelect.press && plusFrame != menuClosedFrame && !customSettings.enableControllerDebugging)
        OpenMenu();
    ++plusFrame;
}

void RSDK::Plus::DrawOverlay()
{
    if (plus.noticeFrames <= 0 || !currentScreen)
        return;
    const std::vector<std::string> lines = WrapText(plus.notice, kTextChars);
    int32 y = currentScreen->size.y - 12 - (int32)lines.size() * kLine;
    DrawPanel(y - 4, (int32)lines.size() * kLine + 6);
    for (const std::string &line : lines) {
        DrawCentered(line, y, kSelectedColor);
        y += kLine;
    }
}

void RSDK::Plus::Notify(const std::string &text)
{
    plus.notice       = text;
    plus.noticeFrames = kNoticeFrames;
}

void RSDK::Plus::Release()
{
    if (IsRetroGame(plus.current) && engine.version != 5 && GlobalExists("game.coinCount"))
        SyncCoins();
    plus.config.lastGame = plus.current;
    SaveConfig();
}
