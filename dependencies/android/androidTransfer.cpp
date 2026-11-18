// =====================
// (Plus) The transfer page on Android: the app's side
// =====================
//
// Connects the transfer server (dependencies/android/transfer, from
// SonicWebMania) to the two halves of the app:
//   - the launcher (Launcher.java), through Transfer.java's native methods:
//     it starts the page on the game's folder, shows its address and PIN, and
//     applies the files staged during the last session before the game starts;
//   - the engine: ConfigureTransferForGame(), called by android_main before
//     the engine runs, gives the Plus menu the page's address and a "Restart
//     game" entry (which applies what the page staged), and connects the
//     page's settings and Sonic Origins conversions to the Plus module, as
//     SonicWebMania's main.cpp does on the TV.

#include <RSDK/Core/RetroEngine.hpp>
#include <main.hpp>

#include "androidTransfer.hpp"
#include "transfer/AndroidTransfer.h"
#include "transfer/NetInfo.h"

#include <iterator>
#include <string>
#include <utility>

using namespace RSDK;

namespace
{

/** "Restart game" was chosen in the Plus menu: android_main restarts the app once the engine has stopped. */
bool restartRequested = false;

/** A Sonic Origins conversion ran last frame (see OnEngineFrame()). */
bool conversionRunning = false;

/** A Java string as UTF-8 ("" for null). */
std::string FromJava(JNIEnv *env, jstring text)
{
    if (!text)
        return std::string();
    const char *chars = env->GetStringUTFChars(text, NULL);
    std::string converted(chars ? chars : "");
    if (chars)
        env->ReleaseStringUTFChars(text, chars);
    return converted;
}

/** The page's state of a conversion, from the Plus module's. */
transfer::android::ConversionState ToConversionState(const Plus::OriginsAudioStatus &status)
{
    transfer::android::ConversionState state;
    state.running = status.running;
    state.percent = status.percent;
    state.message = status.message;
    return state;
}

/**
 * Connects the page's Sonic Origins sections to the Plus module's converters.
 * The page only asks for a conversion while the game runs (not in setup mode).
 */
void SetConversionHooks()
{
    transfer::android::ConversionHooks audio, video;
    audio.fileNames.assign(std::begin(Plus::kOriginsAudioFileNames), std::end(Plus::kOriginsAudioFileNames));
    audio.start  = [] { return Plus::StartOriginsAudioConversion() != 0; };
    audio.status = [] { return ToConversionState(Plus::GetOriginsAudioStatus()); };
    video.fileNames.assign(std::begin(Plus::kOriginsVideoFileNames), std::end(Plus::kOriginsVideoFileNames));
    video.convertible = Plus::OriginsVideoConversionAvailable() != 0;
    video.start       = [] { return Plus::StartOriginsVideoConversion() != 0; };
    video.status      = [] { return ToConversionState(Plus::GetOriginsVideoStatus()); };
    transfer::android::SetConversionHooks(std::move(audio), std::move(video));
}

/**
 * Once per frame, on the engine's thread: a settings change asked for on the
 * page, then the settings as they are now, for the page to show; and the
 * packs a finished conversion wrote, which the game now reads.
 */
void OnEngineFrame()
{
    transfer::android::PageSettings requested;
    if (transfer::android::TakeSettingsRequest(&requested)) {
        Plus::Settings settings;
        settings.plusDLC      = requested.plusDLC;
        settings.maniaAudio   = requested.maniaAudio;
        settings.similarMusic = requested.similarMusic;
        settings.maniaRemixes = requested.maniaRemixes;
        Plus::ApplySettings(settings);
    }

    const Plus::Settings current = Plus::CurrentSettings();
    transfer::android::PageSettings published;
    published.plusDLC      = current.plusDLC;
    published.maniaAudio   = current.maniaAudio;
    published.similarMusic = current.similarMusic;
    published.maniaRemixes = current.maniaRemixes;
    transfer::android::PublishSettings(published);

    const bool converting = Plus::GetOriginsAudioStatus().running || Plus::GetOriginsVideoStatus().running;
    if (conversionRunning && !converting)
        transfer::android::RefreshPacksInUse();
    conversionRunning = converting;
}

/** The Plus menu's "Restart game": stops the engine; android_main then restarts the app (see RestartRequested()). */
void RequestRestart()
{
    restartRequested         = true;
    RenderDevice::isRunning = false;
}

} // namespace

void ConfigureTransferForGame()
{
    if (!transfer::android::Running())
        return;

    transfer::android::EnterGame();

    Plus::PlatformTexts texts;
    const std::string page = transfer::android::PageAddress();
    if (!page.empty()) {
        texts.menuInfo         = "Transfer page: " + page + "  PIN " + transfer::android::Pin();
        texts.packHint         = "Add retro packs and fan games from the transfer page on a phone or computer: " + page + ". Then choose Restart game.";
        texts.originsAudioHint = "Upload them on the transfer page (" + page + "), then press OK here or Convert there.";
    }
    else {
        texts.menuInfo         = "Connect to a network to reach the transfer page.";
        texts.packHint         = "Add retro packs and fan games from the transfer page (connect to a network first), then choose Restart game.";
        texts.originsAudioHint = "Upload them on the transfer page (connect to a network first), then press OK here.";
    }
    texts.originsVideoHint = texts.originsAudioHint;
    Plus::SetPlatformTexts(texts);
    Plus::AddMenuAction("Restart game", &RequestRestart);
    Plus::SetFrameCallback(&OnEngineFrame);
}

bool RestartRequested() { return restartRequested; }

// ---------------------
// Transfer.java's native methods (the launcher's thread)
// ---------------------

extern "C" JNIEXPORT void JNICALL jnifunc(nativeSetLanAddress, Transfer, jstring address)
{
    transfer::SetLanAddress(FromJava(env, address));
}

extern "C" JNIEXPORT jboolean JNICALL jnifunc(nativeStart, Transfer, jstring root)
{
    SetConversionHooks();
    return transfer::android::Start(FromJava(env, root)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL jnifuncN(nativeStop, Transfer) { transfer::android::Stop(); }

extern "C" JNIEXPORT jstring JNICALL jnifuncN(nativePageAddress, Transfer)
{
    return env->NewStringUTF(transfer::android::PageAddress().c_str());
}

extern "C" JNIEXPORT jstring JNICALL jnifuncN(nativePin, Transfer) { return env->NewStringUTF(transfer::android::Pin().c_str()); }

extern "C" JNIEXPORT jboolean JNICALL jnifuncN(nativeHasGameData, Transfer)
{
    return transfer::android::HasGameData() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL jnifuncN(nativeApplyPendingFiles, Transfer) { return transfer::android::ApplyPendingFiles(); }
