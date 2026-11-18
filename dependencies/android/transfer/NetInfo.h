/**
 * @file NetInfo.h
 * @brief Small network helpers: the TV's LAN address, and whether a client
 *        address belongs to a private network.
 */
#ifndef TRANSFER_NET_INFO_H
#define TRANSFER_NET_INFO_H

#include <string>

namespace transfer
{

/**
 * Best guess at the address other devices on the LAN can reach the TV on:
 * the first IPv4 address of an interface that is up and not loopback,
 * preferring wired and Wi-Fi interfaces.
 *
 * @return Dotted-quad address, or an empty string when offline.
 */
std::string FindLanAddress();

/**
 * (Android) Records the device's LAN address for FindLanAddress(). The NDK has
 * getifaddrs() only from Android 7, so the app's Java side looks it up
 * (Launcher.java) and passes it here; "" when offline. Any thread.
 */
void SetLanAddress(const std::string &address);

/**
 * Whether @p address is loopback, link-local or in a private (RFC 1918 /
 * unique-local) range. The transfer server only answers such clients.
 */
bool IsPrivateNetworkAddress(const std::string &address);

} // namespace transfer

#endif // TRANSFER_NET_INFO_H
