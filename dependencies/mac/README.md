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
2. Build the game's library from its own project: for Sonic Mania, `SonicMania.xcodeproj`'s **SonicMania (v5U)** scheme, which makes `Game.dylib`. (The **SonicMania** scheme builds for the older engine; its library does not work with RSDKv5U.)
3. Put `Game.dylib`, the game's `Data.rsdk` (and, for Plus, the `retro` folder) in `~/Library/Application Support/RSDKv5/`. The engine creates that folder on its first run.
