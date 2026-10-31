# Mac

Builds with Xcode (`RSDKv5U.xcodeproj`), for Apple Silicon and Intel, macOS 12 or later.

## Dependencies

Put these in this folder (Git ignores them):

* **SDL2**: the macOS `.dmg` of an SDL **2.x** release (https://github.com/libsdl-org/SDL/releases): copy its `SDL2.framework` here.
* **libogg** and **libtheora**: their sources (https://xiph.org/downloads/) unpacked as `libogg/` and `libtheora/`, then run

  ```
  bash build-frameworks.sh
  ```

  It builds `libogg/macosx/Ogg.framework` and `libtheora/macosx/Theora.framework`, where the Xcode project looks for them, with Xcode's own compiler (no CMake or Homebrew needed): universal (Apple Silicon and Intel), macOS 12 or later, loading from the app's `Frameworks` folder.

## Compiling

1. Open `RSDKv5U.xcodeproj`, set signing to "Sign to Run Locally" if Xcode asks, and build the **RSDKv5U** scheme (Product > Scheme > Edit Scheme > Run > Release for a release build). `RSDKv5.xcodeproj` builds the older RSDKv5 engine instead.
2. Build the game's library from its own project: for Sonic Mania, `SonicMania.xcodeproj`'s **SonicMania** scheme (in Release), which makes `Game.dylib` for RSDKv5U. (**SonicMania (v5)** and **SonicMania (PrePlus)** build for the older engine; their library does not work with RSDKv5U, which then stops at "Mod loading done!".)
3. Put `Game.dylib` and the game's `Data.rsdk` (and, for Plus, the `retro` folder) either:
   * **next to `RSDKv5U.app`**, in any folder (a USB drive works too): the engine uses the app's folder when it holds `Data.rsdk` or `retro`; or
   * in `~/Library/Application Support/RSDKv5/`, used otherwise (created on the first run).

   Settings, saves and `log.txt` go to the same folder.
