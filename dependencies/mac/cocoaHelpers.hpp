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

#endif
