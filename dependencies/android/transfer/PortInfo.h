/**
 * @file PortInfo.h
 * @brief App name, version and project page, shown on the transfer page.
 *
 * (Android) Adapted from SonicWebMania's: the server here is part of Sonic
 * Mania Decompilation Plus' Android app.
 */
#ifndef TRANSFER_PORT_INFO_H
#define TRANSFER_PORT_INFO_H

namespace transfer
{

/** Set by CMake from the engine's DECOMP_VERSION, so the app and the page always agree. */
#ifndef TRANSFER_APP_VERSION
#define TRANSFER_APP_VERSION "0.0.0-dev"
#endif

constexpr const char *kAppName    = "Sonic Mania Decompilation Plus";
constexpr const char *kAppVersion = TRANSFER_APP_VERSION;
/** Where people can find the source, issues and releases. */
constexpr const char *kProjectUrl = "https://github.com/pustal/Sonic-Mania-Decompilation-Plus";
/** The same address without the scheme, for short texts. */
constexpr const char *kProjectUrlShort = "github.com/pustal/Sonic-Mania-Decompilation-Plus";

} // namespace transfer

#endif // TRANSFER_PORT_INFO_H
