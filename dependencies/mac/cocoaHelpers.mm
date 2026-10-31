#ifdef __APPLE__

#import <Foundation/Foundation.h>
#include "cocoaHelpers.hpp"

#include <string.h>

/**
 * Copies @p path into a static buffer. (Plus) The strings Foundation hands
 * out belong to the autorelease pool and are freed when it drains, so they
 * cannot be returned as they are.
 */
static const char* copyPath(NSString *path, char *buffer, size_t size)
{
    buffer[0] = 0;
    if (path)
        strlcpy(buffer, [path fileSystemRepresentation], size);
    return buffer;
}

const char* getResourcesPath(void)
{
    static char buffer[0x400];
    @autoreleasepool
    {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
        NSString *applicationSupportDirectory = [paths firstObject];

        return copyPath(applicationSupportDirectory, buffer, sizeof(buffer));
    }
}

const char* getAppFolderPath(void)
{
    static char buffer[0x400];
    @autoreleasepool
    {
        return copyPath([[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent], buffer, sizeof(buffer));
    }
}
#endif
