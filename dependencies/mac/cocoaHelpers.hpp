#ifndef COCOA_HELPERS_H
#define COCOA_HELPERS_H

/** ~/Library/Application Support, without a trailing slash. Valid until the next call. */
const char* getResourcesPath(void);

/**
 * (Plus) The folder RSDKv5U.app is in, without a trailing slash, for running
 * from a folder that holds the game's files (a USB drive, say). Valid until
 * the next call.
 */
const char* getAppFolderPath(void);

/**
 * (Plus) Takes macOS's own full screen (a Space of its own, from the title
 * bar's green button or the View menu) away from @p nsWindow, an NSWindow, so
 * the green button zooms it instead. macOS offers that full screen to every
 * resizable window, but SDL and the game would not know the window is in it:
 * the Plus menu would still say "Windowed", and SDL crashes when asked for its
 * own full screen from there. Full screen stays on Alt+Enter and the Plus
 * menu's Display setting. Call on the main thread, after the window is made
 * resizable.
 */
void disableNativeFullScreen(void *nsWindow);

/**
 * (Plus) Fetches @p url (HTTPS) with NSURLSession, waiting for the answer:
 * call it on a thread of its own (the update check, PlusUpdate.cpp). The body
 * comes back in @p body (malloc'd, NUL-terminated, for the caller to free;
 * null when there is none), and a failure's reason in @p error.
 *
 * @return The HTTP status (200 on success), or 0 when the request failed.
 */
int httpGetText(const char *url, const char *userAgent, char **body, char *error, unsigned long errorSize);

/** (Plus) Opens @p url in the default browser. */
void openURL(const char *url);

#endif
