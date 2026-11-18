# Transfer page (Android)

The web page the Android app runs on the local network, from which a phone or computer uploads the game's files (Sonic Mania's `Data.rsdk`, the retro packs, Sonic Origins' files, mods), backs up and restores the saves and changes the Plus settings. The launcher shows its address and PIN, and the Plus menu shows them in game.

The server and the page come from [SonicWebMania](https://github.com/pustal/SonicWebMania) (its `src/port` and `web/index.html`), where the same page runs on LG webOS TVs. They are copied here under their licence (`LICENSE-SonicWebMania.md`), with these changes:

- the namespace (`transfer`, was `swm`) and the include guards;
- `NetInfo.cpp`: on Android the LAN address comes from the app's Java side (`SetLanAddress()`), as the NDK has `getifaddrs()` only from Android 7;
- `PortInfo.h`: this app's name, version and project page;
- `index.html`: the wording speaks of "the device" and of the game's Plus menu instead of the TV and its remote.

USB drives are not used on Android (`UsbDrives.cpp` finds none there), so the page offers only the game's folder.

`AndroidTransfer.h` / `.cpp` are this app's own: what the launcher and the engine ask of the server, as SonicWebMania's `main.cpp` does on the TV. The glue to Java and to the engine is `../androidTransfer.cpp`, the launcher's side `android/app/src/main/java/org/rems/rsdkv5/Transfer.java` and `Launcher.java`.

The page is built into the engine's library (`platforms/Android.cmake` embeds `index.html` with `EmbedTextFile.cmake`). The server runs on [cpp-httplib](https://github.com/yhirose/cpp-httplib) 0.63.0 (`httplib.h`, MIT: `LICENSE-cpp-httplib.txt`).
