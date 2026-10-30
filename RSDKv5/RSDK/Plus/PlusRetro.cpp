// =====================
// Plus: save slots, Sonic Origins' modes, coins and achievements for the retro games
// =====================
//
// Included by Plus.cpp.
//
// With only their data packs, the retro games go from the title screen
// straight into the first zone: the 2013 releases of Sonic 1 and 2 drew their
// save slots in native code, and Sonic Origins draws them in its own program.
// The games' scripts still save progress the 2013 way, though, into the
// engine's save memory (SGame.bin): 8 values per slot, slot = options.saveSlot,
//   0 character, 1 lives, 2 score, 3 score bonus,
//   4 stage (regular stage list position + 1; 0x80+ = special stage),
//   5 emeralds (bit mask), 6 special stage position, 7 unused,
// written whenever options.gameMode is 1 (same layout in the Origins and the
// 2013 packs; see RSDKv4-Decompilation's Userdata.hpp and NativeObjects/).
//
// So when a game leaves its title screen for its first zone, the stage load
// is held (OnLegacyStageLoad returns true) and the Plus menu's save screen
// opens over the frozen title:
//   - Sonic 1 and 2 (RSDKv4): its choice is applied as the 2013 menus did: the
//     globals the scripts read, the character and, to continue, the saved
//     stage. The 2013 packs' title screen cannot start a game by itself (its
//     script only runs the demo), so for them the save screen takes the
//     demo's place;
//   - Sonic CD (RSDKv3): its packs contain the 2011 release's own menu (save
//     slots, Time Attack, extras); with the Origins pack, the mode page comes
//     first.
// Once answered, the menu closes and the stage load runs again, this time
// without a save screen.
//
// Sonic Origins' modes: its scripts read game.playMode (0 Classic,
// 1 Anniversary, 2 Boss Rush, 3 Mirror, ...) and game.coinMode (1: an extra
// life becomes a coin, NOTIFY_ADD_COIN, counted in game.coinCount). The coin
// count is kept in Plus.ini, shared by the three games as Origins shares it.
// Mirror mode: the legacy cores mirror the stage before drawing the HUD and
// text, and swap left and right (IsMirrored()). Boss Rush starts at the pack's
// first Boss Rush stage ("BR1..."); Sonic 1's Boss Rush stages are whole acts,
// so, as Origins does, the players start at the act's last checkpoint
// (OnLegacyStageLoaded()).
//
// Achievements: Sonic Mania's list is swapped for the retro game's (the
// numbers its scripts unlock), read from and saved to its Achievements.bin.

namespace RSDK
{
namespace Plus
{

/** Values per save slot in the save memory, and offsets inside a slot. */
const int32 kSlotValues = 8;
enum SlotValue { SLOT_CHARACTER, SLOT_LIVES, SLOT_SCORE, SLOT_SCOREBONUS, SLOT_STAGE, SLOT_EMERALDS, SLOT_SPECIALSTAGE };
/** A stage value at or above this is a special stage. */
const int32 kSpecialStageFlag = 0x80;
/** A new game's values (the 2013 menu's). */
const int32 kNewLives = 3, kNewScoreBonus = 50000;
/** Save slots the save screen offers. */
const int32 kSaveSlotCount = 4;
/** Most coins Origins counts (its HUD has three digits). */
const int32 kMaxCoins = 999;
/** Folder prefix of a pack's first Boss Rush stage, and of every Boss Rush stage. */
const char *const kBossRushFirstFolder  = "BR1";
const char *const kBossRushFolderPrefix = "BR";
/** Objects that are checkpoints, by type name without spaces (Sonic 1: lamp posts, Sonic 2: star posts). */
const char *const kCheckpointTypes[] = { "LampPost", "StarPost" };

const char *const kTitleScene     = "TITLE SCREEN";
const char *const kCdMenuScene    = "MENU";
const char *const kStageMenuScene = "STAGE MENU"; ///< The post-game stage list a cleared save continues at.

/** One of Sonic Origins' modes, as the save screen offers it. */
struct ModeInfo {
    const char *name;
    const char *description;
    int32 playMode; ///< game.playMode (the scripts' BOOT_PLAYMODE_* values).
    bool coins;     ///< game.coinMode: extra lives become coins.
    bool mirrored;  ///< The stage is mirrored and left and right swapped.
    bool bossRush;  ///< Starts at the first Boss Rush stage, without saving.
};

const ModeInfo kModes[] = {
    { "Anniversary", "Extra lives become coins, no time limit.", 1, true, false, false },
    { "Classic", "The original rules: lives and a time limit.", 0, false, false, false },
    // The scripts' own Mirror value (3) pre-flips text for a whole-picture
    // flip; Plus mirrors the stage under the HUD and text instead, so it plays
    // with Anniversary's value.
    { "Mirror", "Anniversary rules, stages mirrored.", 1, true, true, false },
    { "Boss Rush", "The bosses one after another, no saving.", 2, true, false, true },
};
const int32 kModeCount = (int32)(sizeof(kModes) / sizeof(kModes[0]));

/** What the save screen shows for one slot. */
struct SaveSlot {
    bool used = false;
    std::string place; ///< "Green Hill Zone 2", "Special Stage", "Game cleared".
    int32 character = 0;
    int32 lives     = 0;
    int32 emeralds  = 0;
    bool cleared    = false;
};

/** A character a game can be started with. */
struct Character {
    int32 id; ///< The engine's player list position.
    std::string name;
};

/** Everything the save screen shows. */
struct SaveRequest {
    std::string title;
    bool modes        = false; ///< Mode page first (Sonic Origins packs).
    bool slotsEnabled = true;  ///< False: no slot page (Sonic CD has its own menu).
    SaveSlot slots[kSaveSlotCount];
    std::vector<Character> characters; ///< Empty: no character page.
};

/** How the save screen ended. */
enum SaveAction { SAVE_CANCEL, SAVE_NOSAVE, SAVE_NEWGAME, SAVE_CONTINUE };

struct SaveResult {
    SaveAction action = SAVE_CANCEL;
    int32 slot        = -1; ///< For SAVE_NEWGAME and SAVE_CONTINUE.
    int32 character   = 0;  ///< For SAVE_NOSAVE and SAVE_NEWGAME.
    int32 mode        = 0;  ///< Index into kModes when the request had modes.
    std::vector<int32> deleted; ///< Slots deleted on the screen (applied even on cancel).
};

/** Opens the save screen for @p request (PlusMenu.cpp); it calls FinishSaveScreen() when answered. */
void OpenSaveScreen(const SaveRequest &request);
/** Opens the achievement list on its own (PlusMenu.cpp). */
void OpenAchievementsPage();

/** Which start the open save screen is for. */
enum PendingStart { START_NONE, START_SAVESLOTS, START_CDMODES };

/** The retro game session (reset at every game switch). */
struct RetroSession {
    const ModeInfo *mode       = nullptr; ///< Null until a mode is chosen (and always for packs without modes).
    bool mirrorActive          = false;   ///< Mirror mode and a stage is being played.
    bool achievementsInstalled = false;
    bool coinsLoaded           = false; ///< game.coinCount was set from Plus.ini.
    int32 previousCategory     = -1;    ///< The scene that loaded last (to recognise leaving the title).
    std::string previousName;
    PendingStart pendingStart = START_NONE; ///< The save screen is open, the stage load held.
    bool resumeLoad           = false;      ///< The save screen was answered: load without asking again.
};

RetroSession retroSession;

void ResetRetroSession() { retroSession = RetroSession(); }

// ---------------------
// The games' achievements, in the order their scripts number them
// ---------------------

struct RetroAchievement {
    const char *name;
    const char *description;
};

/** Sonic 1 and 2: the 2013 releases' lists (RSDKv4-Decompilation's RetroEngine.cpp), which the Origins packs share. */
const RetroAchievement kSonic1Achievements[] = {
    { "Ramp Ring Acrobatics", "Without touching the ground, collect all the rings in a trapezoid formation in Green Hill Zone Act 1" },
    { "Blast Processing", "Clear Green Hill Zone Act 1 in under 30 seconds" },
    { "Secret of Marble Zone", "Travel through a secret room in Marble Zone Act 3" },
    { "Block Buster", "Break 16 blocks in a row without stopping" },
    { "Ring King", "Collect 200 Rings" },
    { "Secret of Labyrinth Zone", "Activate and ride the hidden platform in Labyrinth Zone Act 1" },
    { "Flawless Pursuit", "Clear the boss in Labyrinth Zone without getting hurt" },
    { "Bombs Away", "Defeat the boss in Starlight Zone using only the see-saw bombs" },
    { "Hidden Transporter", "Collect 50 Rings and take the hidden transporter path in Scrap Brain Act 2" },
    { "Chaos Connoisseur", "Collect all the Chaos Emeralds" },
    { "One For the Road", "As a parting gift, land a final hit on Dr. Eggman's escaping Egg Mobile" },
    { "Beat The Clock", "Clear the Time Attack mode in less than 45 minutes" },
};
const RetroAchievement kSonic2Achievements[] = {
    { "Quick Run", "Complete Emerald Hill Zone Act 1 in under 35 seconds" },
    { "100% Chemical Free", "Complete Chemical Plant without going underwater" },
    { "Early Bird Special", "Collect all the Chaos Emeralds before Chemical Plant" },
    { "Superstar", "Complete any Act as Super Sonic" },
    { "Hit it Big", "Get a jackpot on the Casino Night slot machines" },
    { "Bop Non-stop", "Defeat any boss in 8 consecutive hits without touching the ground" },
    { "Perfectionist", "Get a Perfect Bonus by collecting every Ring in an Act" },
    { "A Secret Revealed", "Find and complete Hidden Palace Zone" },
    { "Head 2 Head", "Win a 2P Versus race against a friend" },
    { "Metropolis Master", "Complete any Metropolis Zone Act without getting hurt" },
    { "Scrambled Egg", "Defeat Dr. Eggman's Boss Attack mode in under 7 minutes" },
    { "Beat the Clock", "Complete the Time Attack mode in less than 45 minutes" },
};
/** Sonic CD: the 2011 release's list (RSDKv3-Decompilation's Userdata.cpp). */
const RetroAchievement kSonicCdAchievements[] = {
    { "88 Miles Per Hour", "Travel through time" },
    { "Just One Hug is Enough", "Get a hug from Amy" },
    { "Paradise Found", "Complete a zone in the Good Future" },
    { "Take the High Road", "Pass the upper Signpost in Collision Chaos Zone 2" },
    { "King of the Rings", "Collect 200 Rings" },
    { "Statue Saviour", "Find the angel statue in Wacky Workbench" },
    { "Heavy Metal", "Defeat Metal Sonic without getting hurt" },
    { "All Stages Clear", "Finish the game" },
    { "Treasure Hunter", "Collect all the Time Stones" },
    { "Dr Eggman Got Served", "Destroy Dr. Eggman's final machine" },
    { "Just In Time", "Complete the Time Attack mode in under 25 minutes" },
    { "Saviour of the Planet", "Destroy all the robot teleporters and Metal Sonic holograms in the past" },
};

/** Size of Achievements.bin: one 32-bit flag per achievement, as the engine writes it. */
const int32 kAchievementSlots = 0x100;

/**
 * Replaces the achievement list with the retro game's, unlocked as its
 * Achievements.bin (in its save folder) says.
 */
void InstallRetroAchievements()
{
    if (retroSession.achievementsInstalled)
        return;
    retroSession.achievementsInstalled = true;

    const std::string &game        = CurrentGameId();
    const RetroAchievement *list = nullptr;
    int32 count                  = 0;
    if (game == "sonic1") {
        list  = kSonic1Achievements;
        count = (int32)(sizeof(kSonic1Achievements) / sizeof(kSonic1Achievements[0]));
    }
    else if (game == "sonic2") {
        list  = kSonic2Achievements;
        count = (int32)(sizeof(kSonic2Achievements) / sizeof(kSonic2Achievements[0]));
    }
    else if (game == "soniccd") {
        list  = kSonicCdAchievements;
        count = (int32)(sizeof(kSonicCdAchievements) / sizeof(kSonicCdAchievements[0]));
    }

    int32 saved[kAchievementSlots];
    memset(saved, 0, sizeof(saved));
    SKU::LoadUserFile("Achievements.bin", saved, sizeof(saved));

    achievementList.clear();
    for (int32 i = 0; i < count && i < kAchievementSlots; ++i) {
        AchievementInfo info;
        info.identifier  = list[i].name;
        info.name        = list[i].name;
        info.description = list[i].description;
        info.achieved    = saved[i] != 0;
        achievementList.push_back(info);
    }
}

// ---------------------
// Scenes, slots and globals
// ---------------------

/** Removes leading and trailing blanks (scene names are padded). */
std::string Trim(const std::string &text)
{
    const size_t first = text.find_first_not_of(' ');
    if (first == std::string::npos)
        return std::string();
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

/** "EMERALD HILL ZONE 2" -> "Emerald Hill Zone 2". */
std::string TitleCase(const std::string &text)
{
    std::string out = text;
    bool wordStart  = true;
    for (char &c : out) {
        c         = (char)(wordStart ? toupper((uint8)c) : tolower((uint8)c));
        wordStart = c == ' ';
    }
    return out;
}

bool IsNumber(const std::string &text)
{
    if (text.empty())
        return false;
    for (const char c : text) {
        if (!isdigit((uint8)c))
            return false;
    }
    return true;
}

/** Name of the scene at absolute list position @p pos. */
std::string SceneName(int32 pos) { return Trim(sceneInfo.listData[pos].name); }

/** Absolute list position of scene @p name in @p category, or -1. */
int32 FindScene(int32 category, const char *name)
{
    const SceneListInfo &list = sceneInfo.listCategory[category];
    for (int32 i = 0; i < list.sceneCount; ++i) {
        if (SceneName(list.sceneOffsetStart + i) == name)
            return list.sceneOffsetStart + i;
    }
    return -1;
}

/** The name of regular stage @p index for people; later acts are listed by number only ("2"). */
std::string RegularStageName(int32 index)
{
    const SceneListInfo &list = sceneInfo.listCategory[Legacy::STAGELIST_REGULAR];
    if (index < 0 || index >= list.sceneCount)
        return "Unknown stage";

    const std::string name = SceneName(list.sceneOffsetStart + index);
    if (!IsNumber(name))
        return TitleCase(name);
    for (int32 i = index - 1; i >= 0; --i) {
        std::string zone = SceneName(list.sceneOffsetStart + i);
        if (IsNumber(zone))
            continue;
        // "EMERALD HILL ZONE 1": drop the act number of the first act.
        const size_t space = zone.find_last_of(' ');
        if (space != std::string::npos && IsNumber(zone.substr(space + 1)))
            zone = zone.substr(0, space);
        return TitleCase(zone + " " + name);
    }
    return TitleCase(name);
}

int32 *SlotValues(int32 slot) { return &Legacy::saveRAM[slot * kSlotValues]; }

bool GlobalExists(const char *name) { return Legacy::GetGlobalVariableID(name) != 0xFF; }

/** True for packs with Sonic Origins' modes. */
bool HasOriginsModes() { return GlobalExists("game.playMode"); }

/** The characters this game offers: the four of every release, plus those Origins added. */
std::vector<Character> CharacterList()
{
    std::vector<Character> characters = { { 0, "Sonic" }, { 1, "Tails" }, { 2, "Knuckles" }, { 3, "Sonic & Tails" } };
    if (GlobalExists("PLAYER_KNUCKLES_TAILS"))
        characters.push_back({ Legacy::GetGlobalVariableByName("PLAYER_KNUCKLES_TAILS"), "Knuckles & Tails" });
    if (GlobalExists("PLAYER_AMY"))
        characters.push_back({ Legacy::GetGlobalVariableByName("PLAYER_AMY"), "Amy" });
    if (GlobalExists("PLAYER_AMY_TAILS"))
        characters.push_back({ Legacy::GetGlobalVariableByName("PLAYER_AMY_TAILS"), "Amy & Tails" });
    return characters;
}

/** Sets the globals of mode @p index (see kModes) and remembers it for mirroring. */
void ApplyMode(int32 index)
{
    const ModeInfo &mode = kModes[index < 0 ? 0 : index >= kModeCount ? kModeCount - 1 : index];
    Legacy::SetGlobalVariableByName("game.playMode", mode.playMode);
    Legacy::SetGlobalVariableByName("game.coinMode", mode.coins ? 1 : 0);
    retroSession.mode = &mode;
    PrintLog(PRINT_NORMAL, "[Plus] %s mode", mode.name);
}

/** Starts Boss Rush at the pack's first Boss Rush stage. @return False if it has none. */
bool StartBossRush()
{
    const SceneListInfo &list = sceneInfo.listCategory[Legacy::STAGELIST_BONUS];
    for (int32 i = 0; i < list.sceneCount; ++i) {
        const char *folder = sceneInfo.listData[list.sceneOffsetStart + i].folder;
        if (strncmp(folder, kBossRushFirstFolder, strlen(kBossRushFirstFolder)) == 0) {
            sceneInfo.activeCategory = Legacy::STAGELIST_BONUS;
            sceneInfo.listPos        = list.sceneOffsetStart + i;
            return true;
        }
    }
    return false;
}

/**
 * Keeps game.coinCount and Plus.ini's coin count in step: the first stage
 * load sets the count, later ones save it when the game changed it.
 */
void SyncCoins()
{
    if (!GlobalExists("game.coinCount"))
        return;
    if (!retroSession.coinsLoaded) {
        Legacy::SetGlobalVariableByName("game.coinCount", Coins());
        retroSession.coinsLoaded = true;
        return;
    }
    int32 now = Legacy::GetGlobalVariableByName("game.coinCount");
    now       = now < 0 ? 0 : now > kMaxCoins ? kMaxCoins : now;
    if (now != Coins())
        SetCoins(now);
}

/** Reads the save memory into what the save screen shows. */
SaveRequest BuildSaveRequest()
{
    SaveRequest request;
    request.title      = gameVerInfo.gameTitle;
    request.characters = CharacterList();
    request.modes      = HasOriginsModes();

    const SceneListInfo &regular = sceneInfo.listCategory[Legacy::STAGELIST_REGULAR];
    for (int32 s = 0; s < kSaveSlotCount; ++s) {
        const int32 *values = SlotValues(s);
        SaveSlot &slot      = request.slots[s];
        slot.used           = values[SLOT_STAGE] != 0;
        if (!slot.used)
            continue;

        slot.character = values[SLOT_CHARACTER];
        slot.lives     = values[SLOT_LIVES];
        for (uint32 bits = (uint32)values[SLOT_EMERALDS]; bits; bits &= bits - 1) ++slot.emeralds;

        const int32 stage = values[SLOT_STAGE];
        if (stage >= kSpecialStageFlag) {
            slot.place = "Special Stage";
        }
        else if (stage - 1 < regular.sceneCount && SceneName(regular.sceneOffsetStart + stage - 1) == kStageMenuScene) {
            slot.place   = "Game cleared";
            slot.cleared = true;
        }
        else {
            slot.place = RegularStageName(stage - 1);
        }
    }
    return request;
}

/** Resets the globals a new game (saved or not) starts from, as the 2013 menu did. */
void SetNewGameGlobals(int32 saveSlot, bool saving)
{
    Legacy::SetGlobalVariableByName("options.attractMode", 0); // A real game, also when the demo was about to start.
    Legacy::SetGlobalVariableByName("options.saveSlot", saveSlot);
    Legacy::SetGlobalVariableByName("options.gameMode", saving ? 1 : 0);
    Legacy::SetGlobalVariableByName("options.stageSelectFlag", 0);
    Legacy::SetGlobalVariableByName("player.lives", kNewLives);
    Legacy::SetGlobalVariableByName("player.score", 0);
    Legacy::SetGlobalVariableByName("player.scoreBonus", kNewScoreBonus);
    Legacy::SetGlobalVariableByName("specialStage.listPos", 0);
    Legacy::SetGlobalVariableByName("specialStage.emeralds", 0);
    Legacy::SetGlobalVariableByName("specialStage.nextZone", 0);
    Legacy::SetGlobalVariableByName("timeAttack.result", 0);
    Legacy::SetGlobalVariableByName("lampPostID", 0);
    Legacy::SetGlobalVariableByName("starPostID", 0);
}

/** Continues @p slot: its globals, character and stage (as the 2013 menu's save select did). */
void ContinueSlot(int32 slot)
{
    const int32 *values = SlotValues(slot);
    Legacy::SetGlobalVariableByName("options.attractMode", 0);
    Legacy::SetGlobalVariableByName("options.saveSlot", slot);
    Legacy::SetGlobalVariableByName("options.gameMode", 1);
    Legacy::SetGlobalVariableByName("options.stageSelectFlag", 0);
    Legacy::SetGlobalVariableByName("player.lives", values[SLOT_LIVES]);
    Legacy::SetGlobalVariableByName("player.score", values[SLOT_SCORE]);
    Legacy::SetGlobalVariableByName("player.scoreBonus", values[SLOT_SCOREBONUS]);
    Legacy::SetGlobalVariableByName("specialStage.listPos", values[SLOT_SPECIALSTAGE]);
    Legacy::SetGlobalVariableByName("specialStage.emeralds", values[SLOT_EMERALDS]);
    Legacy::SetGlobalVariableByName("lampPostID", 0);
    Legacy::SetGlobalVariableByName("starPostID", 0);
    Legacy::v4::playerListPos = values[SLOT_CHARACTER];

    const int32 stage = values[SLOT_STAGE];
    if (stage >= kSpecialStageFlag) {
        Legacy::SetGlobalVariableByName("specialStage.nextZone", stage - (kSpecialStageFlag + 1));
        sceneInfo.activeCategory = Legacy::STAGELIST_SPECIAL;
        sceneInfo.listPos        = sceneInfo.listCategory[Legacy::STAGELIST_SPECIAL].sceneOffsetStart + values[SLOT_SPECIALSTAGE];
    }
    else {
        Legacy::SetGlobalVariableByName("specialStage.nextZone", stage - 1);
        sceneInfo.activeCategory = Legacy::STAGELIST_REGULAR;
        sceneInfo.listPos        = sceneInfo.listCategory[Legacy::STAGELIST_REGULAR].sceneOffsetStart + stage - 1;
    }
}

/** Back to the title screen (the save screen was cancelled). */
void GoToTitle()
{
    const int32 title        = FindScene(Legacy::STAGELIST_PRESENTATION, kTitleScene);
    sceneInfo.activeCategory = Legacy::STAGELIST_PRESENTATION;
    sceneInfo.listPos        = title >= 0 ? title : sceneInfo.listCategory[0].sceneOffsetStart;
}

/** Sonic CD: its own 2011 menu, if the pack has it. @return True if the stage was changed. */
bool GoToCdMenu()
{
    const int32 menu = FindScene(Legacy::STAGELIST_PRESENTATION, kCdMenuScene);
    if (menu < 0)
        return false;
    sceneInfo.activeCategory = Legacy::STAGELIST_PRESENTATION;
    sceneInfo.listPos        = menu;
    return true;
}

/**
 * The save screen was answered (called by the Plus menu, which then closes):
 * applies the choice and lets the held stage load run.
 */
void FinishSaveScreen(const SaveResult &result)
{
    bool changed = false;
    for (const int32 slot : result.deleted) {
        int32 *values = SlotValues(slot);
        for (int32 v = 0; v < kSlotValues; ++v) values[v] = 0;
        changed = true;
    }

    const PendingStart start = retroSession.pendingStart;
    retroSession.pendingStart = START_NONE;
    retroSession.resumeLoad   = true;

    if (result.action == SAVE_CANCEL) {
        GoToTitle();
    }
    else if (start == START_CDMODES) {
        ApplyMode(result.mode);
        bool started = false;
        if (kModes[result.mode].bossRush) {
            Legacy::SetGlobalVariableByName("Options.AttractMode", 0);
            started = StartBossRush();
            if (!started)
                PrintLog(PRINT_NORMAL, "[Plus] This pack has no Boss Rush stages");
        }
        if (!started)
            GoToCdMenu();
    }
    else {
        switch (result.action) {
            default: break;
            case SAVE_NOSAVE:
                SetNewGameGlobals(0, false);
                Legacy::v4::playerListPos = result.character;
                if (HasOriginsModes() && kModes[result.mode].bossRush && !StartBossRush())
                    PrintLog(PRINT_NORMAL, "[Plus] This pack has no Boss Rush stages");
                break;
            case SAVE_NEWGAME: {
                int32 *values                = SlotValues(result.slot);
                values[SLOT_CHARACTER]       = result.character;
                values[SLOT_LIVES]           = kNewLives;
                values[SLOT_SCORE]           = 0;
                values[SLOT_SCOREBONUS]      = kNewScoreBonus;
                values[SLOT_STAGE]           = 1;
                values[SLOT_EMERALDS]        = 0;
                values[SLOT_SPECIALSTAGE]    = 0;
                changed                      = true;
                SetNewGameGlobals(result.slot, true);
                Legacy::v4::playerListPos = result.character;
                break;
            }
            case SAVE_CONTINUE: ContinueSlot(result.slot); break;
        }
        if (HasOriginsModes())
            ApplyMode(result.mode);
    }
    if (changed && !Legacy::WriteSaveRAM())
        PrintLog(PRINT_NORMAL, "[Plus] Could not write the save file");
}

} // namespace Plus
} // namespace RSDK

bool32 RSDK::Plus::OnLegacyStageLoad(int32 engineVersion)
{
    using namespace RSDK::Plus;
    EnsureSaveFolder(); // The mod loader may have reset it since the switch.
    InstallRetroAchievements();
    // Sonic CD's menu offers its Achievements and Leaderboards entries only
    // with an online service; Plus answers them itself.
    if (engineVersion == 3)
        Legacy::onlineActive = true;

    // The save screen is open: keep holding the load.
    if (retroSession.pendingStart != START_NONE)
        return true;

    const bool fromTitle = retroSession.previousCategory == Legacy::STAGELIST_PRESENTATION && retroSession.previousName == kTitleScene;
    const bool toZone    = sceneInfo.activeCategory == Legacy::STAGELIST_REGULAR;
    // The demo the title screen plays when left alone also goes to a zone.
    // Only the Origins packs (they have game.playMode) and Sonic CD start games
    // from their title; for the 2013 Sonic 1 and 2 packs the save screen
    // replaces the demo.
    const bool titleStartsGames = engineVersion == 3 || GlobalExists("game.playMode");
    const bool demo             = titleStartsGames && Legacy::GetGlobalVariableByName(engineVersion == 3 ? "Options.AttractMode" : "options.attractMode") != 0;

    if (retroSession.resumeLoad) {
        // The save screen was just answered: this is the load it held.
        retroSession.resumeLoad = false;
    }
    else if (fromTitle && toZone && !demo) {
        if (engineVersion == 4) {
            Legacy::ReadSaveRAM(); // A missing SGame.bin leaves the memory (all slots empty) as it is.
            retroSession.pendingStart = START_SAVESLOTS;
            OpenSaveScreen(BuildSaveRequest());
            return true;
        }
        if (engineVersion == 3) {
            if (HasOriginsModes()) {
                SaveRequest request;
                request.title        = gameVerInfo.gameTitle;
                request.modes        = true;
                request.slotsEnabled = false; // The 2011 menu has them.
                retroSession.pendingStart = START_CDMODES;
                OpenSaveScreen(request);
                return true;
            }
            GoToCdMenu();
        }
    }
    SyncCoins();

    retroSession.previousCategory = sceneInfo.activeCategory;
    retroSession.previousName     = SceneName(sceneInfo.listPos);

    // Mirror the stages being played, not the title screen or menus.
    retroSession.mirrorActive = retroSession.mode && retroSession.mode->mirrored && sceneInfo.activeCategory != Legacy::STAGELIST_PRESENTATION;
    return false;
}

void RSDK::Plus::OnLegacyStageLoaded(int32 engineVersion)
{
    using namespace RSDK::Plus;
    using namespace RSDK::Legacy;
    if (engineVersion != 4 || !retroSession.mode || !retroSession.mode->bossRush)
        return;
    const char *folder = sceneInfo.listData[sceneInfo.listPos].folder;
    if (sceneInfo.activeCategory != STAGELIST_BONUS || strncmp(folder, kBossRushFolderPrefix, strlen(kBossRushFolderPrefix)) != 0)
        return;
    if (GetGlobalVariableByName("lampPostID") != 0)
        return; // A retry: the checkpoint already brings the players back.

    // The checkpoint furthest along the stage (the bosses wait at the end), if
    // it is ahead of where the stage starts the player: Sonic 1's Boss Rush
    // stages are whole acts, Sonic 2's already start next to the boss.
    const int32 start = v4::objectEntityList[0].xpos;
    int32 checkpoint  = -1;
    for (int32 e = 0; e < LEGACY_v4_TEMPENTITY_START; ++e) {
        const v4::Entity &entity = v4::objectEntityList[e];
        if (!entity.type)
            continue;
        std::string type = v4::typeNames[entity.type];
        type.erase(std::remove(type.begin(), type.end(), ' '), type.end());
        bool isCheckpoint = false;
        for (const char *name : kCheckpointTypes) isCheckpoint = isCheckpoint || type == name;
        if (isCheckpoint && (checkpoint < 0 || entity.xpos > v4::objectEntityList[checkpoint].xpos))
            checkpoint = e;
    }
    if (checkpoint < 0 || v4::objectEntityList[checkpoint].xpos <= start)
        return;

    // Load the stage once more, as after a defeat: the checkpoint then places
    // the players and the camera starts there.
    PrintLog(PRINT_NORMAL, "[Plus] Boss Rush: starting at checkpoint %d", checkpoint);
    SetGlobalVariableByName("lampPostID", checkpoint);
    SetGlobalVariableByName("starPostID", checkpoint);
    stageMode = STAGEMODE_LOAD;
}

bool32 RSDK::Plus::IsMirrored() { return RSDK::Plus::retroSession.mirrorActive; }

void RSDK::Plus::OnLegacyAchievement(int32 id, int32 status)
{
    using namespace RSDK::Plus;
    InstallRetroAchievements();
    if (id < 0 || id >= (int32)achievementList.size() || !status)
        return;
    AchievementInfo &achievement = achievementList[id];
    if (achievement.achieved)
        return;

    achievement.achieved = true;
    SKU::SaveUserData(); // Achievements.bin in the game's save folder.
    PrintLog(PRINT_NORMAL, "[Plus] Achievement unlocked: %s", achievement.name.c_str());
    Notify("Achievement unlocked: " + achievement.name);
}

void RSDK::Plus::ShowLegacyAchievements() { RSDK::Plus::OpenAchievementsPage(); }

void RSDK::Plus::ShowLegacyLeaderboards() { RSDK::Plus::Notify("Leaderboards need an online service"); }
