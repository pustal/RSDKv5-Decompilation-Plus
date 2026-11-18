#ifdef __APPLE__

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include "cocoaHelpers.hpp"

#include <stdlib.h>
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

int httpGetText(const char *url, const char *userAgent, char **body, char *error, unsigned long errorSize)
{
    *body = NULL;
    if (errorSize)
        error[0] = 0;

    @autoreleasepool
    {
        NSURL *address = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
        if (!address) {
            strlcpy(error, "bad address", errorSize);
            return 0;
        }
        NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:address
                                                               cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
                                                           timeoutInterval:20];
        [request setValue:[NSString stringWithUTF8String:userAgent] forHTTPHeaderField:@"User-Agent"];

        __block NSData *received      = nil;
        __block NSInteger status      = 0;
        __block NSString *failure     = nil;
        dispatch_semaphore_t finished = dispatch_semaphore_create(0);
        NSURLSessionDataTask *task =
            [[NSURLSession sharedSession] dataTaskWithRequest:request
                                            completionHandler:^(NSData *data, NSURLResponse *response, NSError *taskError) {
                                                received = data;
                                                if ([response isKindOfClass:[NSHTTPURLResponse class]])
                                                    status = [(NSHTTPURLResponse *)response statusCode];
                                                if (taskError)
                                                    failure = [taskError localizedDescription];
                                                dispatch_semaphore_signal(finished);
                                            }];
        [task resume];
        dispatch_semaphore_wait(finished, DISPATCH_TIME_FOREVER);

        if (failure)
            strlcpy(error, [failure UTF8String], errorSize);
        if (received) {
            const NSUInteger length = [received length];
            *body = (char *)malloc(length + 1);
            if (*body) {
                memcpy(*body, [received bytes], length);
                (*body)[length] = 0;
            }
        }
        return failure ? 0 : (int)status;
    }
}

void openURL(const char *url)
{
    @autoreleasepool
    {
        NSURL *address = [NSURL URLWithString:[NSString stringWithUTF8String:url]];
        if (address)
            [[NSWorkspace sharedWorkspace] openURL:address];
    }
}

void disableNativeFullScreen(void *nsWindow)
{
    if (!nsWindow)
        return;

    @autoreleasepool
    {
        NSWindow *window = (__bridge NSWindow *)nsWindow;

        NSWindowCollectionBehavior behavior = [window collectionBehavior];
        behavior &= ~(NSWindowCollectionBehaviorFullScreenPrimary | NSWindowCollectionBehaviorFullScreenAuxiliary);
        behavior |= NSWindowCollectionBehaviorFullScreenNone;
        [window setCollectionBehavior:behavior];
    }
}
#endif
