/**
 * @file AndroidTransfer.cpp
 * @brief Implementation of the Android facade over the transfer server (see AndroidTransfer.h).
 */
#include "AndroidTransfer.h"

#include "ModLibrary.h"
#include "NetInfo.h"
#include "PlusFiles.h"
#include "RetroGames.h"
#include "SettingsChannel.h"
#include "Storage.h"
#include "TransferServer.h"

#include <android/log.h>

#include <memory>
#include <mutex>
#include <utility>

namespace transfer
{
namespace android
{
namespace
{

/** Tag of this file's lines in Android's log (logcat). */
constexpr const char *kLogTag = "RSDKv5-Transfer";

/**
 * Everything one server needs, for one game folder. The libraries and the
 * server keep references to the storage (and the server to the others), so
 * they live and die together, in this order.
 */
struct Session {
    explicit Session(const std::string &root)
        : storage(root, true), retro(storage), mods(storage), settings(ReadSavedSettings(storage)),
          server(storage, settings, retro, mods, GeneratePin())
    {
    }

    Storage storage;
    RetroLibrary retro;
    ModLibrary mods;
    SettingsChannel settings;
    TransferServer server;
    bool running = false;
};

/** The current session (null before Start() or after Stop()), and its lock: the launcher and the engine reach it from their threads. */
std::mutex sessionMutex;
std::unique_ptr<Session> session;

/** Hooks set before a session exists (SetConversionHooks()), given to each new server. */
ConversionHooks audioHooks;
ConversionHooks videoHooks;

/** The page's OriginsHooks for @p hooks: the engine's converter writes "<game>" + @p packSuffix. */
OriginsHooks ToOriginsHooks(const ConversionHooks &hooks, const char *packSuffix)
{
    OriginsHooks converted;
    converted.fileNames   = hooks.fileNames;
    converted.packSuffix  = packSuffix;
    converted.convertible = hooks.convertible && hooks.start && hooks.status;
    converted.start       = hooks.start ? hooks.start : [] { return false; };
    const std::function<ConversionState()> status = hooks.status;
    converted.status = [status] {
        OriginsState state;
        if (status) {
            const ConversionState current = status();
            state.running                 = current.running;
            state.percent                 = current.percent;
            state.message                 = current.message;
        }
        return state;
    };
    return converted;
}

/**
 * Every pack the game may read this session (see TransferServer::SetPacksInUse()):
 * Sonic Mania's, the retro games' and their converted Sonic Origins audio and videos.
 * As SonicWebMania's main.cpp.
 */
std::vector<std::string> PacksInUse(const Session &current)
{
    std::vector<std::string> packs;
    const std::string maniaPack = current.storage.DataPackPath();
    if (!maniaPack.empty())
        packs.push_back(maniaPack);
    for (const RetroGame &game : current.retro.Scan()) {
        for (const RetroPack *pack : { &game.origins, &game.classic }) {
            if (pack->state == PackState::Ready || pack->state == PackState::Unsupported)
                packs.push_back(pack->path);
        }
    }
    for (const std::string &folder : current.retro.PackFolders()) {
        for (const RetroGameInfo &game : RetroCatalogue()) {
            for (const char *suffix : { TransferServer::kOriginsPackSuffix, TransferServer::kOriginsVideoPackSuffix }) {
                const std::string path = folder + "/" + game.id + suffix;
                if (Storage::FileSize(path) > 0)
                    packs.push_back(path);
            }
        }
    }
    return packs;
}

} // namespace

bool Start(const std::string &root)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (session && session->storage.Root() == Storage(root, true).Root() && session->running)
        return true;

    if (session)
        session->server.Stop();
    session.reset();

    session.reset(new Session(root));
    session->server.SetOriginsHooks(ToOriginsHooks(audioHooks, TransferServer::kOriginsPackSuffix),
                                    ToOriginsHooks(videoHooks, TransferServer::kOriginsVideoPackSuffix));
    session->server.SetSetupMode(true);
    session->running = session->server.Start();
    if (session->running)
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "Transfer page on port %u for %s", static_cast<unsigned>(session->server.Port()),
                            root.c_str());
    else
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "The transfer page could not bind a port");
    return session->running;
}

void Stop()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (session)
        session->server.Stop();
    session.reset();
}

bool Running()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    return session && session->running;
}

std::string PageAddress()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    const std::string address = FindLanAddress();
    if (!session || !session->running || address.empty())
        return std::string();
    return "http://" + address + ":" + std::to_string(session->server.Port());
}

std::string Pin()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    return session && session->running ? session->server.Pin() : std::string();
}

bool HasGameData()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (!session)
        return false;
    return session->storage.HasValidDataPack() || !session->retro.ScanPlayable().empty();
}

int ApplyPendingFiles()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (!session)
        return 0;
    const int applied = session->storage.ApplyPendingFiles() + session->retro.ApplyPendingPacks() + session->mods.ApplyPending();
    session->storage.TrimLog();
    if (applied > 0)
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "Applied %d file(s) uploaded during the last session", applied);
    return applied;
}

void EnterGame()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (!session)
        return;
    session->server.SetPacksInUse(PacksInUse(*session));
    session->server.SetModsInUse(session->mods.ActiveFolders());
    session->server.SetSetupMode(false);
}

void SetConversionHooks(ConversionHooks audio, ConversionHooks video)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    audioHooks = std::move(audio);
    videoHooks = std::move(video);
}

bool TakeSettingsRequest(PageSettings *requested)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    GameSettings settings;
    if (!session || !session->settings.TakeRequest(&settings))
        return false;
    requested->plusDLC      = settings.plusDLC;
    requested->maniaAudio   = settings.maniaAudio;
    requested->similarMusic = settings.similarMusic;
    requested->maniaRemixes = settings.maniaRemixes;
    return true;
}

void PublishSettings(const PageSettings &current)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (!session)
        return;
    GameSettings settings;
    settings.plusDLC      = current.plusDLC;
    settings.maniaAudio   = current.maniaAudio;
    settings.similarMusic = current.similarMusic;
    settings.maniaRemixes = current.maniaRemixes;
    session->settings.Publish(settings);
}

void RefreshPacksInUse()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (session)
        session->server.SetPacksInUse(PacksInUse(*session));
}

} // namespace android
} // namespace transfer
