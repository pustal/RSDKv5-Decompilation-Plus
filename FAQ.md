# General
### Q: Why are the Sonic Mania Plus and Sonic Origins Plus DLCs off?
A: The decompilation's licence asks every build given to anyone to keep the paid content off, to protect Sonic Mania Plus and Sonic Origins Plus and to keep this project from becoming a means of piracy. In this fork they are off by default, and the player turns them on if they own them: in the Plus menu's Settings (**Plus DLC**, press OK twice; the game restarts), or with `plusDLC=y` under `[Game]` in `Settings.ini`. Builds made with `RETRO_DISABLE_PLUS` keep them off for good.

### Q: How do I change the username?
A: In the `Game` category of `Settings.ini`, add a tag called `username` and enter the desired username in it.

### Q: What's the difference between RSDKv5 and RSDKv5U?
A: RSDKv5U adds compatibility for playing RSDKv3 (i.e. Sonic CD) and RSDKv4 (i.e. Sonic 1 & 2) games, including support for local multiplayer. The v5 side of the engine also receives minor changes, such as altered collision and additional functionality. This fork's Plus features (the retro games next to Sonic Mania, the Plus menu) need RSDKv5U and the mod loader.

### Q: The screen is tearing, how do I fix it?
A: Try turning on VSync in `Settings.ini`.

### Q: I'm trying to use a non-Xbox controller on a Windows build, why isn't it working?
A: The default DirectX backends for Windows only support XInput controllers (i.e. Xbox controllers), with no support for DirectInput. To use other controllers, you'll need to [build the decompilation](README.md#how-to-build) with the OpenGL backend by setting the [`RETRO_SUBSYSTEM` build flag](README.md#rsdkv5-flags) to `OGL`. It's also recommended to download a [gamecontrollerdb.txt](https://github.com/mdqinc/SDL_GameControllerDB/blob/master/gamecontrollerdb.txt) file and place it next to the executable file.

### Q: Why aren't videos/filters working while using GL?
A: Sonic Mania's `Data.rsdk` only has the DirectX shaders (this fork lends them to the retro games too), not the OpenGL ones, so they come from a mod you make yourself. Create the following directory structure inside your mods directory:
```
GLShaders/
| Data/
| | ...
| mod.ini
```

Inside `mods/GLShaders/Data/`, copy the `RSDKv5/Shaders` directory, and inside the `mod.ini`, paste this:
```
Name=GLShaders
Description=GL3 shaders
Author=Ducky
Version=1.0.0
TargetVersion=-1
```

### Q: I found a bug/I have a feature request!
A: Submit an issue in [this fork's Issues tab](https://github.com/pustal/RSDKv5-Decompilation-Plus/issues). Keep in mind that this is a decompilation, so bugs that exist in official releases will most likely not be fixed here; problems with the Plus features (the retro games, the Plus menu, the Sonic Origins conversions, the Wii, Wii U and Vita ports) belong here rather than upstream.

# The Plus menu and the retro games
### Q: How do I open the Plus menu?
A: Press **Select** in any game: Tab on a keyboard, Minus on the Wii, Wii U and Switch, SELECT on the Vita, Z on a GameCube controller, MENU in Android's touch controls. On the developer menu's controller debugging, Select is taken, so the Plus menu does not open.

### Q: Where do the retro games' packs go, and under which names?
A: In a `retro` folder next to Sonic Mania's `Data.rsdk`: `sonic1-origins.rsdk`, `sonic2-origins.rsdk` and `soniccd-origins.rsdk` from Sonic Origins, and `sonic1-classic.rsdk`, `sonic2-classic.rsdk` and `soniccd-classic.rsdk` from the earlier releases. [The README](README.md#plus-retro-games-next-to-sonic-mania) says where each one comes from. The Plus menu's Games page shows what each game will play with.

### Q: I have both the Origins and the earlier release's pack. Which one plays?
A: The Origins one. The earlier release's pack then only lends its music and sound effects (Origins keeps its audio outside its packs). Without the Origins pack, the 2013 Sonic 1 and 2 and the mobile Sonic CD play on their own.

### Q: Where are the retro games' saves?
A: Each retro game keeps its saves and achievements in `retro/<game>/` (`retro/sonic1/`, ...). The Plus settings, the coin count and the touch controls' state are in `Plus.ini`.

# Using Sonic Origins RSDK Files
### Q: Why is there no audio?
A: Sonic Origins doesn't have any music or sound effects in the games' data files: it plays all of it itself, through CRIWARE. This fork fills the gap three ways, and uses the first one it finds for each sound:
1. **Sonic Origins' own audio, converted.** Copy its `.acb` and `.awb` files (`image/x64/raw/sound`) into `retro/origins/` and choose **Convert Sonic Origins audio** in the Plus menu's Settings ([the README](README.md#plus-retro-games-next-to-sonic-mania) lists the files).
2. **The earlier release's pack** (`sonic1-classic.rsdk`, ...).
3. **Sonic Mania's**, for the sound effects the others lack and the zones Mania remixed (Plus menu, Settings: **Sonic Mania audio in retro games**).

### Q: Why don't Sonic Origins' cutscenes play?
A: Sonic Origins keeps them in `.usm` files (VP9 video) the engine cannot play. Copy them into `retro/origins/` too and choose **Convert Sonic Origins videos** in the Plus menu's Settings: they become Theora packs (`retro/<game>-origins-video.rsdk`). Use the regular `.usm` files, not the `_4k` ones: every video plays at most at 1024x512. The Wii, Wii U and Vita builds have no video converter (copy packs converted on a computer, phone or TV), and the Wii plays no videos at all yet.

### Q: Why is the Drop Dash disabled? How do I turn it on?
A: The Drop Dash belongs to Sonic Origins' Anniversary mode; the decompilation alone starts the games in Classic mode. With this fork, starting Sonic 1 or 2 opens a save screen that asks for the mode first: choose **Anniversary** (or Mirror, or Boss Rush). Sonic CD asks for it before its own menu.

### Q: How do I play as the Origins Plus characters?
A: Turn the Plus DLC on (see the first question) if you own Sonic Origins Plus: the save screen then offers Amy and Amy & Tails (and Knuckles in Sonic CD). With the DLC off, they are hidden and the engine refuses them.

# Wii, Wii U and PlayStation Vita
### Q: Where does the data go?
A: In an `RSDKv5` folder: `sd:/RSDKv5/` on the Wii and the Wii U, `ux0:data/RSDKv5/` on the Vita, with the retro packs in its `retro` folder. On the Wii, the Homebrew Channel argument `path=/OtherFolder/` reads it from another folder.

### Q: Can I convert Sonic Origins' audio and videos on these consoles?
A: The audio, yes. The videos, no: decoding Sonic Origins' 1080p VP9 videos would take these consoles hours, so convert them on a computer, phone or TV and copy the `-origins-video.rsdk` packs over.

# Miscellaneous
### Q: Is there a decompilation for RSDKv3 and/or RSDKv4 alone?
A: There is! You can find RSDKv3 [here](https://github.com/RSDKModding/RSDKv3-Decompilation) and RSDKv4 [here](https://github.com/RSDKModding/RSDKv4-Decompilation).

### Q: Is this the official RSDKv5 decompilation?
A: No. It is a fork of [RSDKModding's RSDKv5 decompilation](https://github.com/RSDKModding/RSDKv5-Decompilation) that brings the features of [SonicWebMania](https://github.com/pustal/SonicWebMania) (the LG webOS TV port) to every platform. Questions about the decompilation itself, and RSDKModding's own plans, belong to [their repository](https://github.com/RSDKModding/RSDKv5-Decompilation).
