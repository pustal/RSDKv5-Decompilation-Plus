#pragma once

// (Plus) A resizable, movable game window for the Windows renderers (DX9 and DX11).
//
// Upstream opens the windowed mode as a fixed-size popup: 424x240 (1x) when there is no
// Settings.ini, centred, without a frame unless "border" is set, so it can be neither
// moved nor resized. With these helpers the bordered window is a normal one (title bar,
// minimise/maximise buttons, resizable frame), starts at the largest whole multiple of the
// game's resolution that fits the screen comfortably, and rebuilds the renderer when its
// size changes, so the picture always fills it (keeping the game's aspect ratio).
//
// Included by DX9RenderDevice.cpp and DX11RenderDevice.cpp, which supply the rebuild itself
// (RenderDevice::ApplyWindowResize()). Only the windowed mode is affected: fullscreen and the
// borderless window ("border=n" in Settings.ini) behave as before.

#if !RETRO_USE_ORIGINAL_CODE

namespace RSDK
{
namespace Win32Window
{

// The style of the bordered window: an ordinary overlapped window.
constexpr DWORD kBorderedStyle = WS_OVERLAPPEDWINDOW;

// How much of the screen's work area the starting window may cover (per axis).
constexpr float kStartAreaFraction = 0.8f;

// Window-sizing state shared by the message handler and the renderer.
struct SizingState {
    bool32 inSizeMove    = false; // the user is dragging the frame or the title bar
    bool32 resizePending = false; // the size changed during the drag: rebuild when it ends
    int32 appliedWidth   = 0;     // client size the renderer was last built for
    int32 appliedHeight  = 0;
};

// The one SizingState (a function-local static, as the engine may build as C++11).
inline SizingState &Sizing()
{
    static SizingState state;
    return state;
}

// The window's client area size; false when it has none (minimised).
inline bool GetClientSize(HWND window, int32 *width, int32 *height)
{
    RECT client;
    if (!window || !GetClientRect(window, &client))
        return false;

    *width  = client.right - client.left;
    *height = client.bottom - client.top;
    return *width > 0 && *height > 0;
}

// A starting size for a window still at the 1x default (or smaller): the largest whole multiple
// of the game's resolution that fits kStartAreaFraction of the primary screen's work area.
// Windows already set to a larger size (by the user, or saved from a resize) are kept.
inline void FitStartupWindowSize()
{
    if (!videoSettings.windowed || videoSettings.pixWidth <= 0 || videoSettings.pixHeight <= 0)
        return;

    if (videoSettings.windowWidth > videoSettings.pixWidth || videoSettings.windowHeight > videoSettings.pixHeight)
        return;

    RECT workArea;
    if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0))
        return;

    const int32 maxWidth  = (int32)((workArea.right - workArea.left) * kStartAreaFraction);
    const int32 maxHeight = (int32)((workArea.bottom - workArea.top) * kStartAreaFraction);

    int32 scale = 1;
    while (videoSettings.pixWidth * (scale + 1) <= maxWidth && videoSettings.pixHeight * (scale + 1) <= maxHeight) ++scale;

    videoSettings.windowWidth  = videoSettings.pixWidth * scale;
    videoSettings.windowHeight = videoSettings.pixHeight * scale;
}

// WM_GETMINMAXINFO: the window can't be made smaller than the game's resolution (1x).
inline void LimitMinimumSize(HWND window, MINMAXINFO *info)
{
    RECT frame = { 0, 0, videoSettings.pixWidth, videoSettings.pixHeight };
    AdjustWindowRect(&frame, (DWORD)GetWindowLong(window, GWL_STYLE), false);

    info->ptMinTrackSize.x = frame.right - frame.left;
    info->ptMinTrackSize.y = frame.bottom - frame.top;
}

// Whether a WM_SIZE should make the renderer follow the window: only for a window that is up
// and running (not while the renderer itself is being set up or refreshed), in windowed mode.
inline bool ShouldFollowSize(WPARAM sizeType)
{
    return videoSettings.windowState != WINDOWSTATE_UNINITIALIZED && videoSettings.windowed && sizeType != SIZE_MINIMIZED;
}

// Records the window's new size after the renderer was rebuilt for it: the restored (not
// maximised) size becomes the window size kept in Settings.ini.
// Returns whether the size differs from the one the renderer was built for.
inline bool TakeNewSize(HWND window, int32 *width, int32 *height)
{
    Sizing().resizePending = false;

    if (!GetClientSize(window, width, height))
        return false;

    if (*width == Sizing().appliedWidth && *height == Sizing().appliedHeight)
        return false;

    if (!IsZoomed(window)) {
        videoSettings.windowWidth  = *width;
        videoSettings.windowHeight = *height;
    }
    return true;
}

// After the renderer was (re)built: the size it was built for.
inline void NoteAppliedSize(HWND window)
{
    if (!GetClientSize(window, &Sizing().appliedWidth, &Sizing().appliedHeight)) {
        Sizing().appliedWidth  = 0;
        Sizing().appliedHeight = 0;
    }
}

} // namespace Win32Window
} // namespace RSDK

#endif // !RETRO_USE_ORIGINAL_CODE
