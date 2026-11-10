![header](header.png?raw=true)

A complete decompilation of Retro Engine v5 and v5Ultimate that ports features I introduced in my [SonicWebMania](https://github.com/pustal/SonicWebMania) project.

Fork of [RSDKModding](https://github.com/RSDKModding/)'s of [RSDK v5U Decompilation](https://github.com/RSDKModding/RSDKv5-Decompilation).


# **SUPPORT THE DEVELOPERS OF THE RETRO ENGINE**
We do not own the Retro Engine in any way, shape or form, and this project would not have been possible had they not developed RSDKv5(U) in the first place. Retro Engine is currently owned by [Evening Star](https://eveningstar.studio/); we highly urge you to follow & support their projects if you enjoyed this project of ours!

## **DO NOT USE THIS DECOMPILATION PROJECT AS A MEANS TO PIRATE SONIC MANIA (PLUS) OR ANY OTHER RSDKv5(U) GAMES.**
We do not condone using this project as a means for piracy in any form. This project was made with love and care for the source material and was created for purely educational purposes.

# Additional Tweaks
* Added a built-in mod loader and API allowing to easily create and play mods with features such as save file redirection and XML asset loading, supported by all sub-versions of v5U.
* Added a built-in shader compiler for backends/platforms that support it.
* Added various other backends to windows aside from the usual DirectX 9 backends

## If you are here for Sonic Mania, Sonic Origins, the old Sonic 1 & 2 Classic or 2012's Sonic CD:
You have the option of building RSDKv5 Plus alongside these games in [the Sonic Mania Decompilation Plus repo](https://github.com/pustal/Sonic-Mania-Decompilation-Plus).

# Plus: retro games next to Sonic Mania
This fork (RSDKv5-Decompilation-Plus) adds, on top of the decompilation, a way to play **Sonic the Hedgehog, Sonic 2 and Sonic CD** next to Sonic Mania from your own data packs, managed from an in-game menu. It needs v5U (`RETRO_REVISION` 3) and the mod loader, and can be switched off with `RETRO_PLUS`. No game data is included.

* **Retro games.** Put the packs in a `retro` folder next to Sonic Mania's `Data.rsdk`, under these names:

  | File | Where it comes from |
  |---|---|
  | `sonic1-origins.rsdk`, `sonic2-origins.rsdk`, `soniccd-origins.rsdk` | `Sonic1u.rsdk`, `Sonic2u.rsdk`, `SonicCDu.rsdk` from Sonic Origins' `retro` folder |
  | `sonic1-classic.rsdk`, `sonic2-classic.rsdk` | the 2013 releases' `Data.rsdk` (Android: `Data.rsdk.xmf` in the APK) |
  | `soniccd-classic.rsdk` | Sonic CD (2011): the mobile app's `Data.rsdk.xmf`, or Steam's `Data.rsdk`. Converted in place to the format the engine opens the first time it is found |

  With the Origins pack, a game is the Origins version and the earlier release's pack lends it its music and sounds. Without it, the 2013 Sonic 1 and 2 and the mobile Sonic CD play on their own (Sonic CD's Steam pack can only lend its sound: its scripts are stored under the PC build's names, which the engine does not load).
* **Plus menu.** Press **Select** (Tab on a keyboard) in any game: Resume, Games, Achievements, Settings, Exit. It is drawn with the engine's developer font, so it works on every renderer.
* **The official Plus DLCs are off by default**, as the decompilation's licence asks of every build given to anyone: Sonic Mania Plus (Encore mode, Mighty, Ray...) and Sonic Origins Plus (Amy, and Knuckles in Sonic CD). If you own them, turn them on in the Plus menu's Settings (**Plus DLC**, press OK twice; the game restarts) or with `plusDLC=y` under `[Game]` in `Settings.ini`, which works in every build, with or without the Plus menu. Builds made with `RETRO_DISABLE_PLUS` keep them off.
* **Switching games** from the Games page happens in-process (as the mod loader reloads a game), so it works on every platform. The engine starts with the last game played.
* **Save slots and Sonic Origins' modes.** Starting Sonic 1 or 2 opens a save screen: a mode (with the Origins packs: Anniversary, Classic, Mirror, Boss Rush), four save slots (continue, new game, delete) and the character (with Origins: Knuckles & Tails too, and Amy and Amy & Tails with the Plus DLC on). Sonic CD goes to its own 2011 menu, after the mode with the Origins pack. Coins are shared by the three games, as in Origins. Mirror mode mirrors the stages and swaps left and right while the HUD stays readable; Boss Rush starts each act next to its boss.
* **Achievements.** Each retro game has its own twelve achievements (those of the 2013 and 2011 releases), listed in the Plus menu, with a notice when one unlocks. Sonic CD's menu entry opens the same list.
* **Sonic Origins' own audio.** Sonic Origins plays the retro games' music and sounds with CRIWARE, from its own files, not from its packs. Copy these from its `image/x64/raw/sound` folder into a folder named `origins` inside `retro`: `HITE_sfx.acb`, and for each game its `_sfx.acb`, `_music.acb` and `_music.awb` (`STH1` Sonic 1, `STH2` Sonic 2, `SCD` Sonic CD). Then choose **Convert Sonic Origins audio** in the Plus menu's Settings: it writes `retro/<game>-origins-audio.rsdk` for each game (about 30, 70 and 250 MB) and restarts a retro game that is running. The files in `origins` can be deleted afterwards. The converted packs hold Sonic Origins' HCA audio without its encryption, under the names the games ask for, and the engine plays HCA (`RSDKv5/RSDK/Audio/HcaAudio.hpp`, decoding by [clHCA from vgmstream](dependencies/all/clHCA/LICENSE)): music loops at Sonic Origins' own loop points, and the speed-shoes versions play 1.2 times faster, as in Origins. Sounds then come from Sonic Origins first, then from the earlier release's pack, then from Sonic Mania's. Which sound each file is comes from Sonic Origins' own table, `retro_rfl_param.rfl`: `tools/make-origins-audio-table.py` writes `RSDKv5/RSDK/Plus/PlusOriginsTable.hpp` (names only) from your copy of `raw/rfl/rfl_resident.pac`.
* **Sonic Origins' videos.** Origins keeps its cutscenes in `.usm` files (VP9 video, encrypted HCA sound) in `image/x64/raw/movie`. Copy these into the same `retro/origins` folder: `soniccd_op.usm`, `cd_ed_good.usm`, `cd_ed_bad.usm`, `hite_sonic1_intro.usm`, `hite_sonic1_outro.usm`, `hite_sonic2_intro.usm`, `hite_sonic2_outro.usm`, `hite_soniccd_intro.usm`, `hite_soniccd_outro.usm` and `sonicteam_logo.usm` (not the `_4k` or `_anim` copies). Then choose **Convert Sonic Origins videos** in the Plus menu's Settings: it decodes every frame (libvpx's VP9 decoder, `dependencies/all/libvpx`), scales it to 360 lines and encodes it as Theora, and writes `retro/<game>-origins-video.rsdk`. That takes minutes on a computer and much longer on a TV or console; a retro game that is running restarts when it is done. Sonic CD then plays its own opening and endings when its scripts ask for them (with the soundtrack chosen in its options), and in every game Origins' animated intro plays when a new game starts, its outro before the credits that follow the ending, and the Sonic Team logo before the first title screen; Start, A or B skips them. Sonic 1 and 2 gained video playback for this (RSDKv4 had none). Converting needs libvpx and Theora's encoder: the CMake builds have both (`PLUS_VIDEO_CONVERTER`, on by default; Linux needs `libtheora-dev`'s `theoraenc`); the Xcode and Visual Studio projects build without it and play packs converted elsewhere.
* **Sonic Mania's audio.** The Origins packs have no music or sounds, and even with the earlier releases' packs the sounds Origins added (the Drop Dash, Knuckles' and Amy's moves) are missing. Sonic Mania's pack fills those gaps: the same sound effects (or the nearest ones) and the zones Mania remixed (Green Hill, Chemical Plant, Oil Ocean, Stardust Speedway, Metallic Madness and the jingles), with Mania's loop points (Green Hill, Chemical Plant and Oil Ocean play Mania's act 1 remix in act 1 and its act 2 remix from act 2 on). The game's own packs (and Sonic Origins' converted audio) win, with two optional settings:
  * **Similar Mania music** gives the other zones a Mania track of similar feel (Emerald Hill plays Green Hill, Casino Night plays Studiopolis...);
  * **Sonic Mania remixes** plays the remixed zones' Mania version even when the earlier release's pack has the original.
* **Touch controls (Android).** A small **PAD** button at the top right shows or hides a pad, A, B, START and MENU (the Plus menu) over the game; it shows when the app opens and when pressed, then fades out but keeps working. Their opacity (100% to 0%, still working unseen) is in the Plus menu's Settings. Build with `-DPLUS_TOUCH_CONTROLS=1` to try them on another platform.
* **Saves.** Each retro game keeps its saves and achievements in `retro/<game>/`; settings, the coin count and the touch controls' state in `Plus.ini`.
* **Fixes in the legacy cores:** scripts that read buttons without naming a controller (Sonic CD 2011's menus) now get the player's input; Sonic 1 and 2's 2013 packs (RSDKvB) open, and their Ogg sound effects play.

The code is in `RSDKv5/RSDK/Plus/` (one translation unit, included by `RetroEngine.cpp`); the engine calls it from places marked `(Plus)`.

**Icons.** Every platform's app icon (macOS, Windows, Android, the Switch, and the window's icon on Linux) is the R of the RSDK logo with the green PLUS, drawn from `header.png` by `tools/make-icons.py` (Pillow and NumPy), which writes them all again if the header changes.

A port that wraps the engine in its own launcher can tell the Plus module what only it knows, through the platform API at the end of `RSDKv5/RSDK/Plus/Plus.hpp`: more folders to find the retro packs in (a USB drive's, say), a line for the menu's main page and the Games page's and Settings page's hints, starting the Sonic Origins audio conversion and following it, its own main-page entries, a per-frame callback on the engine's thread, and the menu's settings to read and apply. [SonicWebMania](https://github.com/pustal/SonicWebMania), the LG webOS TV app, uses it with no other change to the engine.


# How to Build

This project uses [CMake](https://cmake.org/), a versatile building system that supports many different compilers and platforms. You can download CMake [here](https://cmake.org/download/). **(Make sure to enable the feature to add CMake to the system PATH during the installation if you're on Windows!)**

## Get the source code

**DO NOT** download the source code ZIP archive from GitHub, as they do not include the submodules required to build the decompilation.

Instead, you will need to clone the repository using Git, which you can get [here](https://git-scm.com/downloads).

Clone the repo **recursively**, using:
`git clone --recursive https://github.com/pustal/RSDKv5-Decompilation-Plus`

If you've already cloned the repo, run this command inside of the repository:
```git submodule update --init```

## Getting dependencies

### Windows
To handle dependencies, you'll need to install [Visual Studio Community](https://visualstudio.microsoft.com/downloads/) (make sure to install the `Desktop development with C++` package during the installation) and [vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started?pivots=shell-cmd#1---set-up-vcpkg) (You only need to follow `1 - Set up vcpkg`).

After installing those, run the following in Command Prompt (make sure to replace `[vcpkg root]` with the path to the vcpkg installation!):
- `[vcpkg root]/vcpkg.exe install libtheora libogg glew glfw3 sdl2 --triplet=x64-windows-static` (If you're compiling a 32-bit build, replace `x64-windows-static` with `x86-windows-static`.)

Finally, follow the [compilation steps below](#compiling) using `-DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static` as arguments for `cmake -B build`.
  - Make sure to replace `[vcpkg root]` with the path to the vcpkg installation!
  - If you're compiling a 32-bit build, replace `x64-windows-static` with `x86-windows-static`.

### Linux
Install the following dependencies: then follow the [compilation steps below](#compiling):
- **pacman (Arch):** `sudo pacman -S base-devel cmake glew glfw libtheora`
- **apt (Debian/Ubuntu):** `sudo apt install build-essential cmake libglew-dev libglfw3-dev libtheora-dev`
- **rpm (Fedora):** `sudo dnf install make cmake gcc glew-devel glfw-devel libtheora-devel zlib-devel`
- **xbps (Void):** `sudo xbps-install make cmake gcc pkg-config glew-devel glfw-devel libtheora-devel zlib-devel`
- Your favorite package manager here, [make a pull request](https://github.com/RSDKModding/RSDKv5-Decompilation/fork) (also update [Mania](https://github.com/RSDKModding/Sonic-Mania-Decompilation)!)

#### (make sure to [install GL shaders!](FAQ.md#q-why-arent-videosfilters-working-while-using-gl))

### Switch
[Setup devKitPro](https://devkitpro.org/wiki/Getting_Started), then run the following:
- `(dkp-)pacman -Syuu switch-dev switch-libogg switch-libtheora switch-sdl2 switch-glad`

Finally, follow the [compilation steps below](#compiling) using `-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake` as arguments for `cmake -B build`.

#### (make sure to [install GL shaders!](FAQ.md#q-why-arent-videosfilters-working-while-using-gl))

### Android
Follow the android build instructions [here.](./dependencies/android/README.md)

## Compiling

Compiling is as simple as typing the following in the root repository directory:
```
cmake -B build
cmake --build build --config release
```

The resulting build will be located somewhere in `build/` depending on your system.

The following cmake arguments are available when compiling:
- Use these by adding `-D[flag-name]=[value]` to the end of the `cmake -B build` command. For example, to build with `RETRO_DISABLE_PLUS` set to on, add `-DRETRO_DISABLE_PLUS=on` to the command.

### RSDKv5 flags
- `RETRO_REVISION`: What revision to compile for. Takes an integer, defaults to `3` (RSDKv5U).
- `RETRO_DISABLE_PLUS`: Forces the Plus DLC off, with no way to turn it on. Takes a boolean (on/off), defaults to `off`. Without it the DLC is still off by default and only the player turns it on (Settings.ini `Game:plusDLC`, or the Plus menu), as the licence allows.
- `RETRO_MOD_LOADER`: Enables or disables the mod loader. Takes a boolean, defaults to `on`.
- `RETRO_MOD_LOADER_VER`: Manually sets the mod loader version. Takes an integer, defaults to the current latest version.
- `RETRO_PLUS`: Enables this fork's Plus features (retro games, the Plus menu, Sonic Mania's audio for the retro games). Needs `RETRO_REVISION` 3 and the mod loader. Not the Sonic Mania Plus DLC (see `RETRO_DISABLE_PLUS`). Takes a boolean, defaults to `on`.
- `RETRO_DISABLE_LOG`: Disables the log. Not recommended unless it impacts performance. Takes a boolean, defaults to `off`.
- `RETRO_SUBSYSTEM`: *Only change this if you know what you're doing.* Changes the subsystem that RSDKv5 will be built for. Defaults to the most standard subsystem for the platform.

# FAQ
You can find the FAQ [here](./FAQ.md).
