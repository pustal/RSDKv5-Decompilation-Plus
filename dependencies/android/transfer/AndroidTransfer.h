/**
 * @file AndroidTransfer.h
 * @brief The transfer page on Android: what the app's launcher and the engine
 *        ask of the transfer server (TransferServer.h), in one place.
 *
 * The server and the libraries around it come from SonicWebMania (its
 * src/port), unchanged but for this namespace and a few Android notes. This
 * facade does here what SonicWebMania's main.cpp does on the TV:
 *   - the launcher (Launcher.java, through androidTransfer.cpp) starts the
 *     server on the game's folder once one is chosen, shows its address and
 *     PIN, waits for the game's files when there are none yet, and applies the
 *     files staged during the last session before the game starts;
 *   - the engine (android_main) then tells the Plus module the page's address
 *     and gives the page the settings and the Sonic Origins conversions, once
 *     per frame, on its own thread.
 *
 * The server works on ordinary file paths (std::filesystem), so the game's
 * folder must be one: the app's own folder always is; a folder picked with
 * Android's folder picker is when it is on the device's storage and the app
 * may reach it directly (Android's "All files access"). Launcher.java finds
 * the path, or leaves the page off.
 *
 * This header needs only C++11, so the engine's code can include it; the
 * implementation is built as C++17 (platforms/Android.cmake).
 */
#ifndef TRANSFER_ANDROID_TRANSFER_H
#define TRANSFER_ANDROID_TRANSFER_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace transfer
{
namespace android
{

/** The settings the page offers, as the engine's Plus module has them (RSDK::Plus::Settings). */
struct PageSettings {
    bool plusDLC      = false;
    bool maniaAudio   = true;
    bool similarMusic = false;
    bool maniaRemixes = false;
};

/** A Sonic Origins conversion's progress, as the engine reports it. */
struct ConversionState {
    bool running = false;
    int percent  = 0;
    std::string message;
};

/** What the page needs to start and follow one kind of Sonic Origins conversion (its audio, its videos). */
struct ConversionHooks {
    std::vector<std::string> fileNames;      ///< The files the engine's converter reads.
    bool convertible = true;                 ///< This build converts them.
    std::function<bool()> start;             ///< Starts a conversion; false when one runs. Any thread.
    std::function<ConversionState()> status; ///< Any thread.
};

/**
 * Starts the server on the game's folder @p root (an absolute path), in setup
 * mode: files uploaded now are installed at once. Restarting it on another
 * folder stops the previous one. Launcher thread.
 *
 * @return True when it is serving (see Port()).
 */
bool Start(const std::string &root);

/** Stops the server, if it runs. */
void Stop();

/** Whether the server is serving. */
bool Running();

/** The page's address, "http://<LAN address>:<port>", or "" when it is off or the device is offline. */
std::string PageAddress();

/** The session's PIN, or "" when the server is off. */
std::string Pin();

/** Whether the game's folder holds something to play: Sonic Mania's Data.rsdk, a playable retro pack or a fan game. */
bool HasGameData();

/**
 * Before the game starts: moves the files staged during the last session into
 * place (a new Data.rsdk or retro pack, saves, mod changes) and trims the
 * engine's log, as SonicWebMania does at launch. Nothing reads them yet then.
 *
 * @return The number of files applied.
 */
int ApplyPendingFiles();

/**
 * The game starts: from now on uploads that would replace something the game
 * reads are staged for the next start, and the packs and mods it may read are
 * protected.
 */
void EnterGame();

/**
 * Connects the page's Sonic Origins sections to the engine's converters.
 * Call before Start(): a server reads them while it serves, so a running one
 * keeps those it started with. Before the game runs (setup mode), the page
 * uploads the files but does not convert them.
 */
void SetConversionHooks(ConversionHooks audio, ConversionHooks video);

/**
 * Once per frame, engine thread: takes a settings change asked for on the page
 * (true and fills @p requested when there is one), after which the engine
 * applies it and calls PublishSettings() with what it then has.
 */
bool TakeSettingsRequest(PageSettings *requested);

/** Once per frame, engine thread: the settings as they are now, for the page to show. */
void PublishSettings(const PageSettings &current);

/** A conversion wrote new packs, which the game now reads: the page leaves them alone. Engine thread. */
void RefreshPacksInUse();

} // namespace android
} // namespace transfer

#endif // TRANSFER_ANDROID_TRANSFER_H
