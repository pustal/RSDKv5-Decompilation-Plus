// =====================
// (Plus) "Check for updates" on Android
// =====================
//
// The Plus menu's entry: the app looks for a newer release on GitHub and
// installs it when the player agrees (Updater.java, through the game
// activity's checkForUpdates()). Android shows its own dialogs over the game.

#include <RSDK/Core/RetroEngine.hpp>
#include <main.hpp>

#include "androidUpdate.hpp"

using namespace RSDK;

namespace
{

/** The menu entry, on the engine's thread (the menu has closed): hands over to Java, which works on its own threads. */
void CheckForUpdates()
{
    JNISetup *setup              = GetJNISetup();
    const jmethodID checkUpdates = setup->env->GetMethodID(setup->clazz, "checkForUpdates", "()V");
    if (checkUpdates)
        setup->env->CallVoidMethod(setup->thiz, checkUpdates);
}

} // namespace

void ConfigureUpdates() { Plus::AddMenuAction("Check for updates", &CheckForUpdates); }
