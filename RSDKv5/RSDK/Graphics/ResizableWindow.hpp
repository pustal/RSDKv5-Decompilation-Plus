#pragma once

// (Plus) A resizable game window for the SDL2 and GLFW renderers on the desktop (Linux, macOS
// and Windows), as Win32Window.hpp does for the DirectX renderers.
//
// Upstream opens the windowed mode at a fixed size: 424x240 (1x) when there is no
// Settings.ini (and SDL2 switches to fullscreen when the window is maximised). With these
// helpers the bordered window can be resized and maximised like any other, starts at the
// largest whole multiple of the game's resolution that fits the screen comfortably, and the
// renderer is rebuilt when its size changes, so the picture always fills it, keeping the game's
// aspect ratio. The size it is left at (not maximised) is kept in Settings.ini (winWidth,
// winHeight).
//
// Only the windowed mode is affected: fullscreen and the borderless window ("border=n" in
// Settings.ini) behave as before. This header holds what both renderers share; each asks its
// own library for sizes and supplies the rebuild (RenderDevice::ApplyWindowResize()).
// Included by Drawing.cpp before the renderer's source; RETRO_RESIZABLE_WINDOW, whether this
// build has it, is set in Drawing.hpp.

#if RETRO_RESIZABLE_WINDOW

namespace RSDK
{
namespace ResizableWindow
{

// How much of the screen's usable area the starting window may cover (per axis).
constexpr float kStartAreaFraction = 0.8f;

// Frames to wait after the last size change before writing Settings.ini, so dragging the
// frame (which reports a new size for every step) writes it once.
constexpr int32 kSaveDelayFrames = 30;

// Window-sizing state shared by the window's events and the renderer.
struct SizingState {
    bool32 resizePending = false; // the size changed: rebuild once the frame's events are handled
    bool32 following     = false; // the renderer is being rebuilt for a resize: keep the window as it is
    int32 appliedWidth   = 0;     // window size the renderer was last built for
    int32 appliedHeight  = 0;
    int32 saveCountdown  = 0;     // frames left before Settings.ini is written (0: nothing to write)
};

// The one SizingState (a function-local static, as the engine may build as C++11).
inline SizingState &Sizing()
{
    static SizingState state;
    return state;
}

// Whether the windowed mode's window can be resized: the bordered one only, as on Windows.
inline bool Resizable() { return videoSettings.windowed && videoSettings.bordered; }

// A starting size for a window still at the 1x default (or smaller): the largest whole multiple
// of the game's resolution that fits kStartAreaFraction of the screen's usable area
// (@p usableWidth x @p usableHeight, without docks, taskbars and panels).
// Windows already set to a larger size (by the user, or saved from a resize) are kept.
inline void FitStartupWindowSize(int32 usableWidth, int32 usableHeight)
{
    if (!videoSettings.windowed || videoSettings.pixWidth <= 0 || videoSettings.pixHeight <= 0 || usableWidth <= 0 || usableHeight <= 0)
        return;

    if (videoSettings.windowWidth > videoSettings.pixWidth || videoSettings.windowHeight > videoSettings.pixHeight)
        return;

    const int32 maxWidth  = (int32)(usableWidth * kStartAreaFraction);
    const int32 maxHeight = (int32)(usableHeight * kStartAreaFraction);

    int32 scale = 1;
    while (videoSettings.pixWidth * (scale + 1) <= maxWidth && videoSettings.pixHeight * (scale + 1) <= maxHeight) ++scale;

    videoSettings.windowWidth  = videoSettings.pixWidth * scale;
    videoSettings.windowHeight = videoSettings.pixHeight * scale;
}

// Whether a size change should make the renderer follow the window: only for a window that is
// up and running (not while the renderer itself is being set up or refreshed), in windowed mode.
inline bool ShouldFollowSize() { return videoSettings.windowState != WINDOWSTATE_UNINITIALIZED && videoSettings.windowed; }

// Takes the window's new size (@p width x @p height) before the renderer is rebuilt for it.
// @p restored: the window is neither maximised nor minimised, so this size becomes the window
// size kept in Settings.ini.
// Returns whether the size differs from the one the renderer was built for.
inline bool TakeNewSize(int32 width, int32 height, bool restored)
{
    Sizing().resizePending = false;

    if (width <= 0 || height <= 0)
        return false;

    if (width == Sizing().appliedWidth && height == Sizing().appliedHeight)
        return false;

    if (restored) {
        videoSettings.windowWidth  = width;
        videoSettings.windowHeight = height;
        Sizing().saveCountdown     = kSaveDelayFrames;
    }
    return true;
}

// After the renderer was (re)built: the window size it was built for.
inline void NoteAppliedSize(int32 width, int32 height)
{
    Sizing().appliedWidth  = width;
    Sizing().appliedHeight = height;
}

// Once per frame: whether the last size change is old enough to be written to Settings.ini.
inline bool SaveDue()
{
    if (Sizing().saveCountdown <= 0)
        return false;
    return --Sizing().saveCountdown == 0;
}

} // namespace ResizableWindow
} // namespace RSDK

#endif // RETRO_RESIZABLE_WINDOW
