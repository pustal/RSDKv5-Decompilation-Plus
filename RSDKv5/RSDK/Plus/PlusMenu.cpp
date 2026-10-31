// =====================
// Plus: the Plus menu
// =====================
//
// Included by Plus.cpp. Built on the developer menu's machinery: opening it
// (OpenDevMenu) pauses whichever game runs, Sonic Mania or a legacy one, and
// each page is a devMenu.state function run once per frame over the frozen
// picture, drawn with the engine's developer font (DrawDevString) so it shows
// on every renderer. Pages:
//   - Main: Resume, Games, Achievements, Settings, Exit;
//   - Games: Sonic Mania and the retro games found, to switch to;
//   - Achievements: the running game's list;
//   - Settings: Sonic Mania audio in the retro games, similar music, Mania's
//     remixes, and the touch controls (Android);
//   - the save screen of the retro games (PlusRetro.cpp): mode, slots, a
//     slot's actions, character.
// Select (Tab on a keyboard) opens and closes it; Back goes up a page.

namespace RSDK
{
namespace Plus
{

/** Colours, as the developer menu uses them. */
const uint32 kBoxColor      = 0x000080;
const uint32 kTitleColor    = 0xF0F080;
const uint32 kSelectedColor = 0xF0F0F0;
const uint32 kItemColor     = 0x808090;
const uint32 kDimColor      = 0x505060;
const uint32 kGoodColor     = 0x80F080;
const uint32 kWarnColor     = 0xF08080;

/** Line height and the widest text that fits a panel (the font is 8 pixels wide). */
const int32 kLine      = 10;
const int32 kPanelW    = 0x150;
const int32 kTextChars = (kPanelW - 16) / 8;
/** Rows a scrolling list shows at once. */
const int32 kListRows = 10;

/** The menu's state between frames. */
struct MenuState {
    int32 selection = 0;
    int32 scroll    = 0;
    std::string message; ///< Shown under a page once (an error, a confirmation).

    bool achievementsOnly = false; ///< Opened from Sonic CD's menu: Back closes the menu.

    // Save screen.
    SaveRequest request;
    SaveResult result;
    int32 pendingSlot  = -1;
    bool deleteArmed   = false;
};

MenuState menu;

/** The frame the menu last closed on, so the same Select press does not reopen it. */
uint32 menuClosedFrame = 0xFFFFFFFF;

/** One frame's menu input. */
struct MenuInput {
    bool up      = false;
    bool down    = false;
    bool left    = false;
    bool right   = false;
    bool confirm = false;
    bool back    = false;
    bool close   = false;
};

/** Up/Down repeat while held, as in the developer menu. */
bool Repeat(const InputState &key)
{
    if (key.press) {
        devMenu.timer = 1;
        return true;
    }
    if (!key.down)
        return false;
    if (devMenu.timer) {
        devMenu.timer = (devMenu.timer + 1) & 7;
        return false;
    }
    devMenu.timer = (devMenu.timer + 1) & 7;
    return true;
}

MenuInput ReadMenuInput()
{
    const ControllerState &pad = controller[CONT_ANY];
    MenuInput input;
    input.up    = Repeat(pad.keyUp);
    input.down  = Repeat(pad.keyDown);
    input.left  = pad.keyLeft.press;
    input.right = pad.keyRight.press;

    const bool flip = SKU::GetConfirmButtonFlip();
    input.confirm   = pad.keyStart.press || (flip ? pad.keyB.press : pad.keyA.press);
    input.back      = flip ? pad.keyA.press : pad.keyB.press;
    input.close     = pad.keySelect.press;
    return input;
}

/** Moves @p selection by the input over @p count items. */
void MoveSelection(const MenuInput &input, int32 count)
{
    if (count <= 0)
        return;
    if (input.up)
        menu.selection = (menu.selection + count - 1) % count;
    if (input.down)
        menu.selection = (menu.selection + 1) % count;
}

/** Splits @p text into lines of at most @p width characters, at spaces. */
std::vector<std::string> WrapText(const std::string &text, int32 width)
{
    std::vector<std::string> lines;
    std::string line;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find(' ', pos);
        if (end == std::string::npos)
            end = text.size();
        const std::string word = text.substr(pos, end - pos);
        if (!line.empty() && (int32)(line.size() + 1 + word.size()) > width) {
            lines.push_back(line);
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
        pos = end + 1;
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

/** A dark box behind the menu, centred, from @p y for @p height pixels. */
void DrawPanel(int32 y, int32 height) { DrawRectangle(currentScreen->center.x - kPanelW / 2, y, kPanelW, height, kBoxColor, 0xFF, INK_NONE, true); }

/** Centred text. */
void DrawCentered(const std::string &text, int32 y, uint32 color) { DrawDevString(text.c_str(), currentScreen->center.x, y, ALIGN_CENTER, color); }

/** The title box every page starts with. @return The y the page's body starts at. */
int32 DrawHeader(const char *title, const std::string &subtitle)
{
    int32 y = 12;
    DrawPanel(y - 4, subtitle.empty() ? 16 : 26);
    DrawCentered(title, y, kTitleColor);
    if (!subtitle.empty())
        DrawCentered(subtitle, y + kLine, kItemColor);
    return y + (subtitle.empty() ? 20 : 30);
}

/** Wrapped help or description text in its own box at @p y. @return The y after it. */
int32 DrawNote(const std::string &text, int32 y, uint32 color)
{
    const std::vector<std::string> lines = WrapText(text, kTextChars);
    if (lines.empty())
        return y;
    DrawPanel(y - 4, (int32)lines.size() * kLine + 6);
    for (const std::string &line : lines) {
        DrawCentered(line, y, color);
        y += kLine;
    }
    return y + 8;
}

/** A list of @p items with the selection highlighted, scrolling past kListRows. @return The y after it. */
int32 DrawList(const std::vector<std::string> &items, const std::vector<uint32> &colors, int32 y)
{
    const int32 count = (int32)items.size();
    if (menu.selection < menu.scroll)
        menu.scroll = menu.selection;
    if (menu.selection >= menu.scroll + kListRows)
        menu.scroll = menu.selection - kListRows + 1;

    const int32 shown = count < kListRows ? count : kListRows;
    DrawPanel(y - 4, shown * kLine + 6);
    for (int32 i = menu.scroll; i < menu.scroll + shown; ++i) {
        const uint32 color = i == menu.selection ? kSelectedColor : colors[i];
        DrawCentered((i == menu.selection ? "> " : "") + items[i] + (i == menu.selection ? " <" : ""), y, color);
        y += kLine;
    }
    return y + 8;
}

/**
 * The paused game's picture: the pages draw over a copy of it every frame,
 * so a shorter page does not leave a longer one's text behind.
 */
std::vector<uint16> menuBackground;

/** Keeps the picture the menu opens over. */
void CaptureBackground()
{
    const uint16 *pixels = screens[0].frameBuffer;
    menuBackground.assign(pixels, pixels + SCREEN_XMAX * SCREEN_YSIZE);
}

/** Puts the paused picture back before a page draws. */
void BeginPage()
{
    currentScreen = &screens[0];
    if (menuBackground.size() == (size_t)(SCREEN_XMAX * SCREEN_YSIZE))
        memcpy(screens[0].frameBuffer, menuBackground.data(), menuBackground.size() * sizeof(uint16));
}

/** Leaves the menu (the game resumes). */
void CloseMenu()
{
    menu.message.clear();
    menuClosedFrame = plusFrame;
    CloseDevMenu();
}

void Page_Main();
void Page_Games();
void Page_Achievements();
void Page_Settings();
void Page_SaveModes();
void Page_SaveSlots();
void Page_SlotActions();
void Page_Characters();

/** Shows page @p page from its first item. */
void GoToPage(void (*page)())
{
    devMenu.state  = page;
    menu.selection = 0;
    menu.scroll    = 0;
    menu.message.clear();
}

// ---------------------
// Main
// ---------------------

void Page_Main()
{
    BeginPage();
    const std::vector<std::string> items = { "Resume", "Games", "Achievements", "Settings", "Exit" };
    const std::vector<uint32> colors(items.size(), kItemColor);

    int32 y = DrawHeader("PLUS MENU", gameVerInfo.gameTitle);
    y       = DrawList(items, colors, y);
    DrawNote("SELECT: close  BACK: resume", y, kDimColor);

    const MenuInput input = ReadMenuInput();
    MoveSelection(input, (int32)items.size());
    if (input.close || input.back) {
        CloseMenu();
        return;
    }
    if (!input.confirm)
        return;
    switch (menu.selection) {
        case 0: CloseMenu(); break;
        case 1: GoToPage(Page_Games); break;
        case 2:
            menu.achievementsOnly = false;
            GoToPage(Page_Achievements);
            break;
        case 3: GoToPage(Page_Settings); break;
        case 4: RenderDevice::isRunning = false; break;
    }
}

// ---------------------
// Games
// ---------------------

/** A row of the Games page. */
struct GameRow {
    std::string id;
    std::string title;
    std::string description;
    bool playable;
};

std::vector<GameRow> GameRows()
{
    std::vector<GameRow> rows;
    rows.push_back({ kManiaId, "Sonic Mania", ManiaAvailable() ? "Sonic Mania (Data.rsdk)" : "Not installed: Data.rsdk is missing", ManiaAvailable() });
    for (const RetroGame &game : RetroGames()) rows.push_back({ game.info.id, game.info.title, game.Description(), game.Playable() });
    return rows;
}

void Page_Games()
{
    BeginPage();
    const std::vector<GameRow> rows = GameRows();
    std::vector<std::string> items;
    std::vector<uint32> colors;
    for (const GameRow &row : rows) {
        items.push_back(row.title + (row.id == CurrentGameId() ? " (playing)" : ""));
        colors.push_back(row.playable ? kItemColor : kDimColor);
    }

    int32 y = DrawHeader("GAMES", "Switch to another game");
    y       = DrawList(items, colors, y);
    const GameRow &selected = rows[menu.selection < (int32)rows.size() ? menu.selection : 0];
    y = DrawNote(selected.description, y, selected.playable ? kItemColor : kWarnColor);
    if (!menu.message.empty())
        y = DrawNote(menu.message, y, kWarnColor);
    DrawNote("Retro packs go in the \"retro\" folder next to Data.rsdk: sonic1-origins.rsdk, sonic1-classic.rsdk, ...", y, kDimColor);

    const MenuInput input = ReadMenuInput();
    MoveSelection(input, (int32)rows.size());
    if (input.close) {
        CloseMenu();
        return;
    }
    if (input.back) {
        GoToPage(Page_Main);
        menu.selection = 1;
        return;
    }
    if (!input.confirm)
        return;
    if (!selected.playable) {
        menu.message = selected.title + " cannot be played: " + selected.description;
        return;
    }
    if (selected.id == CurrentGameId()) {
        CloseMenu();
        return;
    }
    RequestSwitch(selected.id);
    CloseMenu();
}

// ---------------------
// Achievements
// ---------------------

void Page_Achievements()
{
    BeginPage();
    std::vector<std::string> items;
    std::vector<uint32> colors;
    int32 unlocked = 0;
    for (const AchievementInfo &info : achievementList) {
        items.push_back(info.name);
        colors.push_back(info.achieved ? kGoodColor : kItemColor);
        unlocked += info.achieved ? 1 : 0;
    }

    char subtitle[0x40];
    sprintf_s(subtitle, sizeof(subtitle), "%d of %d unlocked", unlocked, (int32)items.size());
    int32 y = DrawHeader("ACHIEVEMENTS", subtitle);
    if (items.empty()) {
        DrawNote("This game has no achievements.", y, kItemColor);
    }
    else {
        y                           = DrawList(items, colors, y);
        const AchievementInfo &info = achievementList[menu.selection];
        DrawNote(info.description + (info.achieved ? " (unlocked)" : ""), y, info.achieved ? kGoodColor : kItemColor);
    }

    const MenuInput input = ReadMenuInput();
    MoveSelection(input, (int32)items.size());
    if (input.close || (input.back && menu.achievementsOnly)) {
        CloseMenu();
        return;
    }
    if (input.back) {
        GoToPage(Page_Main);
        menu.selection = 2;
    }
}

// ---------------------
// Settings
// ---------------------

/** The Settings page's entries (which ones show depends on the platform). */
enum SettingsItem {
    SETTING_MANIAAUDIO,
    SETTING_SIMILARMUSIC,
    SETTING_REMIXES,
    SETTING_TOUCHCONTROLS,
    SETTING_TOUCHOPACITY,
    SETTING_BACK,
};

/** The Settings page's entries, in order. */
std::vector<SettingsItem> SettingsItems()
{
    std::vector<SettingsItem> items = { SETTING_MANIAAUDIO, SETTING_SIMILARMUSIC, SETTING_REMIXES };
    if (TouchControlsAvailable()) {
        items.push_back(SETTING_TOUCHCONTROLS);
        items.push_back(SETTING_TOUCHOPACITY);
    }
    items.push_back(SETTING_BACK);
    return items;
}

/** "ON" or "OFF". */
const char *OnOff(bool on) { return on ? "ON" : "OFF"; }

void Page_Settings()
{
    BeginPage();
    const std::vector<SettingsItem> ids = SettingsItems();
    std::vector<std::string> items;
    std::vector<uint32> colors;
    for (SettingsItem id : ids) {
        switch (id) {
            case SETTING_MANIAAUDIO: items.push_back(std::string("Sonic Mania audio in retro games: ") + OnOff(ManiaAudioEnabled())); break;
            case SETTING_SIMILARMUSIC: items.push_back(std::string("Similar Mania music: ") + OnOff(SimilarMusicEnabled())); break;
            case SETTING_REMIXES: items.push_back(std::string("Sonic Mania remixes: ") + OnOff(RemixesEnabled())); break;
            case SETTING_TOUCHCONTROLS: items.push_back(std::string("Touch controls: ") + OnOff(TouchControlsShown())); break;
            case SETTING_TOUCHOPACITY: items.push_back("Touch controls opacity: " + std::to_string(TouchOpacity()) + "%"); break;
            case SETTING_BACK: items.push_back("Back"); break;
        }
        // The music options need Sonic Mania's audio.
        const bool needsAudio = id == SETTING_SIMILARMUSIC || id == SETTING_REMIXES;
        colors.push_back(needsAudio && !ManiaAudioEnabled() ? kDimColor : kItemColor);
    }

    int32 y = DrawHeader("SETTINGS", "");
    y       = DrawList(items, colors, y);
    const SettingsItem selected = ids[menu.selection];
    const char *note            = "";
    switch (selected) {
        case SETTING_MANIAAUDIO:
            note = "With Sonic Mania's Data.rsdk: the sounds a retro game's packs lack (all of them with the Origins pack "
                   "alone; the Drop Dash, Knuckles' and Amy's moves otherwise) and the zones Mania remixed.";
            break;
        case SETTING_SIMILARMUSIC:
            note = "Zones Mania has no version of play a Mania track of similar feel (Emerald Hill plays Green Hill, "
                   "Casino Night Studiopolis...).";
            break;
        case SETTING_REMIXES:
            note = "Green Hill, Chemical Plant, Oil Ocean, Stardust Speedway and Metallic Madness play Sonic Mania's "
                   "remix even when the earlier release's pack has the original.";
            break;
        case SETTING_TOUCHCONTROLS: note = "A pad and buttons over the game. The button at the top right also shows and hides them."; break;
        case SETTING_TOUCHOPACITY: note = "Left and right change it; at 0% the controls still work, unseen."; break;
        case SETTING_BACK: break;
    }
    y = DrawNote(note, y, kItemColor);
    if (selected <= SETTING_REMIXES)
        DrawNote("Applies the next time a retro game starts.", y, kDimColor);

    const MenuInput input = ReadMenuInput();
    MoveSelection(input, (int32)items.size());
    if (input.close) {
        CloseMenu();
        return;
    }
    if (input.back || (input.confirm && selected == SETTING_BACK)) {
        GoToPage(Page_Main);
        menu.selection = 3;
        return;
    }
    if (!(input.confirm || input.left || input.right))
        return;
    switch (selected) {
        case SETTING_MANIAAUDIO: SetManiaAudioEnabled(!ManiaAudioEnabled()); break;
        case SETTING_SIMILARMUSIC:
            if (ManiaAudioEnabled())
                SetSimilarMusicEnabled(!SimilarMusicEnabled());
            break;
        case SETTING_REMIXES:
            if (ManiaAudioEnabled())
                SetRemixesEnabled(!RemixesEnabled());
            break;
        case SETTING_TOUCHCONTROLS: SetTouchControlsShown(!TouchControlsShown()); break;
        case SETTING_TOUCHOPACITY: StepTouchOpacity(input.left ? -1 : 1); break;
        case SETTING_BACK: break;
    }
}

// ---------------------
// The retro games' save screen
// ---------------------

/** Ends the save screen with @p action (the menu closes and the held stage load runs). */
void EndSaveScreen(SaveAction action, int32 slot, int32 character)
{
    menu.result.action    = action;
    menu.result.slot      = slot;
    menu.result.character = character;
    FinishSaveScreen(menu.result);
    CloseMenu();
}

/** Whether the chosen mode goes on to the save slots (Boss Rush does not). */
bool ModeUsesSaves() { return !menu.request.modes || !kModes[menu.result.mode].bossRush; }

/** After the mode page: the slots, the character, or the start. */
void AfterModeChosen()
{
    if (menu.request.slotsEnabled && ModeUsesSaves()) {
        GoToPage(Page_SaveSlots);
        // Start on the first slot in use, so continuing is one press.
        for (int32 s = 0; s < kSaveSlotCount; ++s) {
            if (menu.request.slots[s].used) {
                menu.selection = s + 1;
                break;
            }
        }
        return;
    }
    menu.pendingSlot = -1;
    if (!menu.request.characters.empty()) {
        GoToPage(Page_Characters);
        return;
    }
    EndSaveScreen(SAVE_NOSAVE, -1, 0);
}

/** Back from a page: the previous page, or the title screen from the first. */
void BackFromSavePage()
{
    if (devMenu.state == Page_Characters && menu.request.slotsEnabled && ModeUsesSaves()) {
        GoToPage(Page_SaveSlots);
        return;
    }
    if (menu.request.modes && devMenu.state != Page_SaveModes) {
        GoToPage(Page_SaveModes);
        menu.selection = menu.result.mode;
        return;
    }
    EndSaveScreen(SAVE_CANCEL, -1, 0);
}

void Page_SaveModes()
{
    BeginPage();
    std::vector<std::string> items;
    for (const ModeInfo &mode : kModes) items.push_back(mode.name);
    const std::vector<uint32> colors(items.size(), kItemColor);

    int32 y = DrawHeader(menu.request.title.c_str(), "Choose a mode");
    y       = DrawList(items, colors, y);
    y       = DrawNote(kModes[menu.selection].description, y, kItemColor);
    DrawNote("OK: choose  BACK: title screen", y, kDimColor);

    const MenuInput input = ReadMenuInput();
    MoveSelection(input, (int32)items.size());
    if (input.back) {
        EndSaveScreen(SAVE_CANCEL, -1, 0);
        return;
    }
    if (input.confirm) {
        menu.result.mode = menu.selection;
        AfterModeChosen();
    }
}

/** A character's name from its id. */
std::string CharacterName(int32 id)
{
    for (const Character &character : menu.request.characters) {
        if (character.id == id)
            return character.name;
    }
    return "Sonic";
}

void Page_SaveSlots()
{
    BeginPage();
    std::vector<std::string> items = { "No save" };
    std::vector<uint32> colors     = { kItemColor };
    for (int32 s = 0; s < kSaveSlotCount; ++s) {
        const SaveSlot &slot = menu.request.slots[s];
        char line[0x80];
        if (slot.used)
            sprintf_s(line, sizeof(line), "%d: %s", s + 1, slot.place.c_str());
        else
            sprintf_s(line, sizeof(line), "%d: New game", s + 1);
        items.push_back(line);
        colors.push_back(slot.used ? kItemColor : kDimColor);
    }

    int32 y = DrawHeader(menu.request.title.c_str(), menu.request.modes ? kModes[menu.result.mode].name : "Choose a save");
    y       = DrawList(items, colors, y);
    if (menu.selection > 0 && menu.request.slots[menu.selection - 1].used) {
        const SaveSlot &slot = menu.request.slots[menu.selection - 1];
        char details[0x80];
        sprintf_s(details, sizeof(details), "%s, %d lives, %d emerald%s", CharacterName(slot.character).c_str(), slot.lives, slot.emeralds,
                  slot.emeralds == 1 ? "" : "s");
        y = DrawNote(details, y, kItemColor);
    }
    else {
        y = DrawNote(menu.selection == 0 ? "Play without saving." : "Start a new game in this slot.", y, kItemColor);
    }
    DrawNote("OK: choose  BACK: return", y, kDimColor);

    const MenuInput input = ReadMenuInput();
    MoveSelection(input, (int32)items.size());
    if (input.back) {
        BackFromSavePage();
        return;
    }
    if (!input.confirm)
        return;
    menu.pendingSlot = menu.selection - 1;
    if (menu.pendingSlot >= 0 && menu.request.slots[menu.pendingSlot].used) {
        GoToPage(Page_SlotActions);
        menu.deleteArmed = false;
    }
    else {
        GoToPage(Page_Characters);
    }
}

void Page_SlotActions()
{
    BeginPage();
    const std::vector<std::string> items = { "Continue", menu.deleteArmed ? "Delete: press OK again" : "Delete", "Back" };
    const std::vector<uint32> colors     = { kItemColor, menu.deleteArmed ? kWarnColor : kItemColor, kItemColor };

    char subtitle[0x80];
    sprintf_s(subtitle, sizeof(subtitle), "Slot %d: %s", menu.pendingSlot + 1, menu.request.slots[menu.pendingSlot].place.c_str());
    const int32 y = DrawHeader(menu.request.title.c_str(), subtitle);
    DrawList(items, colors, y);

    const MenuInput input = ReadMenuInput();
    const int32 before    = menu.selection;
    MoveSelection(input, (int32)items.size());
    if (menu.selection != before)
        menu.deleteArmed = false;
    if (input.back) {
        GoToPage(Page_SaveSlots);
        menu.selection = menu.pendingSlot + 1;
        return;
    }
    if (!input.confirm)
        return;
    switch (menu.selection) {
        case 0: EndSaveScreen(SAVE_CONTINUE, menu.pendingSlot, 0); break;
        case 1:
            if (!menu.deleteArmed) {
                menu.deleteArmed = true; // Ask for a second press.
                break;
            }
            menu.request.slots[menu.pendingSlot] = SaveSlot();
            menu.result.deleted.push_back(menu.pendingSlot);
            GoToPage(Page_SaveSlots);
            menu.selection = menu.pendingSlot + 1;
            break;
        case 2:
            GoToPage(Page_SaveSlots);
            menu.selection = menu.pendingSlot + 1;
            break;
    }
}

void Page_Characters()
{
    BeginPage();
    std::vector<std::string> items;
    for (const Character &character : menu.request.characters) items.push_back(character.name);
    const std::vector<uint32> colors(items.size(), kItemColor);

    const int32 y = DrawHeader(menu.request.title.c_str(), menu.pendingSlot >= 0 ? "New game: choose a character" : "Choose a character");
    DrawList(items, colors, y);

    const MenuInput input = ReadMenuInput();
    MoveSelection(input, (int32)items.size());
    if (input.back) {
        BackFromSavePage();
        return;
    }
    if (input.confirm && !items.empty())
        EndSaveScreen(menu.pendingSlot < 0 ? SAVE_NOSAVE : SAVE_NEWGAME, menu.pendingSlot, menu.request.characters[menu.selection].id);
}

/** Opens the save screen for @p request over the frozen title (PlusRetro.cpp holds the stage load meanwhile). */
void OpenSaveScreen(const SaveRequest &request)
{
    CaptureBackground();
    OpenDevMenu();
    menu.request = request;
    menu.result  = SaveResult();
    GoToPage(request.modes ? Page_SaveModes : request.slotsEnabled ? Page_SaveSlots : Page_Characters);
    if (devMenu.state == Page_SaveSlots)
        AfterModeChosen();
    PrintLog(PRINT_NORMAL, "[Plus] Save screen open");
}

/** Opens the achievement list on its own (Sonic CD's menu entry); Back returns to the game. */
void OpenAchievementsPage()
{
    CaptureBackground();
    OpenDevMenu();
    GoToPage(Page_Achievements);
    menu.achievementsOnly = true;
}

} // namespace Plus
} // namespace RSDK

void RSDK::Plus::OpenMenu()
{
    CaptureBackground();
    OpenDevMenu();
    GoToPage(Page_Main);
}
