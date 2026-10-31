#ifndef PLUS_H
#define PLUS_H

/**
 * @file Plus.hpp
 * @brief The Plus features of this engine fork (not to be confused with the
 *        Sonic Mania Plus DLC): Sonic the Hedgehog, Sonic 2 and Sonic CD next
 *        to Sonic Mania, from the player's own data packs, managed from the
 *        Plus menu.
 *
 * - Retro games: v5U's legacy cores run the retro games' packs, kept in a
 *   "retro" folder next to Sonic Mania's Data.rsdk under fixed names
 *   (sonic1-origins.rsdk, sonic1-classic.rsdk, ... see PlusPacks.cpp). The
 *   Sonic Origins pack has the game but no audio; the earlier releases' pack
 *   ("classic") lends its music and sounds, or plays on its own (the 2013
 *   Sonic 1 and 2, and Sonic CD's mobile pack).
 * - Switching games happens in-process, the way the mod loader reloads a
 *   game: the packs are swapped and the engine re-initialised (Plus.cpp).
 * - Save slots and Sonic Origins' modes (Anniversary, Classic, Mirror, Boss
 *   Rush), coins and achievements for the retro games (PlusRetro.cpp). Each
 *   retro game keeps its saves in "retro/<game>/".
 * - Sonic Mania's pack lends the retro games the music and sound effects
 *   their packs lack (PlusAudio.cpp).
 * - The Plus menu (PlusMenu.cpp), opened with Select (Tab on a keyboard),
 *   drawn with the engine's developer font so it works on every renderer.
 * - Touch controls on Android (PlusTouch.cpp): a button that fades out,
 *   showing a pad and buttons over the game, with an adjustable opacity.
 *
 * The engine calls the functions below from a few places, all marked
 * "(Plus)" and guarded by RETRO_USE_PLUS.
 */

#include <string>
#include <vector>

namespace RSDK
{
namespace Plus
{

// =====================
// Engine lifecycle
// =====================

/**
 * Called by LoadSettingsINI() once the main data pack (Sonic Mania's,
 * Game:dataFile) has been tried. Reads Plus.ini, finds the retro packs and,
 * when the last game played was a retro game (or Sonic Mania's pack is
 * missing), loads that game's packs instead.
 *
 * @param maniaDataFile Game:dataFile, Sonic Mania's pack.
 * @param maniaLoaded   Whether it loaded.
 * @param useBuffer     LoadDataPack()'s buffering on this platform.
 * @return True when a retro game's packs were loaded in its place.
 */
bool32 OnStartupPacks(const char *maniaDataFile, bool32 maniaLoaded, bool32 useBuffer);

/** Per frame, after the engine has run: the Plus menu's button, notices, a pending game switch. */
void Update();

/** Per frame, before the frame is shown: draws the current notice and the touch controls over the game. */
void DrawOverlay();

/**
 * Called by ProcessInput() once the input devices have been read: the touch
 * controls' button and, when shown, their pad and buttons, which press the
 * same keys a controller would (PlusTouch.cpp). Does nothing on platforms
 * without them.
 */
void ApplyTouchControls();

/** True when a game switch chosen in the Plus menu waits to be applied (outside the menu). */
bool32 SwitchPending();

/** Swaps the packs and re-initialises the engine for the chosen game (Plus.cpp). */
void ApplyPendingSwitch();

/** Saves what the session still holds (Plus.ini, coins). Called at shutdown. */
void Release();

// =====================
// Plus menu
// =====================

/** Opens the Plus menu over the running game. */
void OpenMenu();

/** Shows a notice over the game for a few seconds. */
void Notify(const std::string &text);

// =====================
// Legacy core hooks
// =====================

/**
 * A legacy stage is about to load (engineVersion 3 or 4). When a retro game
 * leaves its title screen for its first zone, opens the save screen (Sonic
 * 1 and 2) or the mode page (Sonic Origins' Sonic CD).
 *
 * @return True to hold the load until the save screen has been answered
 *         (the stage stays in STAGEMODE_LOAD and the hook runs again).
 */
bool32 OnLegacyStageLoad(int32 engineVersion);

/** A legacy stage's objects have loaded: Boss Rush starts next to the boss. */
void OnLegacyStageLoaded(int32 engineVersion);

/** Sonic Origins' Mirror mode is on and a stage is being played. */
bool32 IsMirrored();

/** A retro game's script unlocked achievement @p id (@p status non-zero). */
void OnLegacyAchievement(int32 id, int32 status);

/** Sonic CD's menu asked for the achievement list. */
void ShowLegacyAchievements();

/** Sonic CD's menu asked for the leaderboards, which need an online service. */
void ShowLegacyLeaderboards();

/**
 * A retro game is loading sound effect @p name (relative to Data/SoundFX/):
 * when its packs lack it, picks a Sonic Mania stand-in (PlusAudio.cpp).
 *
 * @return True with the name to load in @p out (at most @p outSize bytes).
 */
bool32 ResolveLegacySfx(const char *name, char *out, int32 outSize);

/**
 * A retro game is starting music track @p name (relative to Data/Music/)
 * that loops at sample @p *loopPoint: when its packs lack it, picks a Sonic
 * Mania stand-in with its own loop point, or silence; with "Sonic Mania
 * remixes" on, the zones Mania remixed play the remix even when the packs
 * have the original (PlusAudio.cpp).
 *
 * @return True with the track in @p out ("" to play nothing).
 */
bool32 ResolveLegacyMusic(const char *name, char *out, int32 outSize, int32 *loopPoint);

} // namespace Plus
} // namespace RSDK

#endif // PLUS_H
