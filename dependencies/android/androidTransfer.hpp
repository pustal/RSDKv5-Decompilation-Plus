#ifndef ANDROID_TRANSFER_HPP
#define ANDROID_TRANSFER_HPP

// (Plus) The transfer page on Android, the engine's side (androidTransfer.cpp).

/**
 * Before the engine runs (android_main): when the launcher started the
 * transfer page, protects what the game will read, puts the page's address in
 * the Plus menu with a "Restart game" entry, and connects the page's settings
 * and Sonic Origins conversions to the Plus module. Does nothing otherwise.
 */
void ConfigureTransferForGame();

/** Whether "Restart game" was chosen in the Plus menu: the engine stopped so the app restarts (android_main). */
bool RestartRequested();

#endif // ANDROID_TRANSFER_HPP
