// =====================
// Plus: touch controls (Android)
// =====================
//
// Included by Plus.cpp. Sonic Mania (the PC game) and the retro games'
// Origins packs expect a controller; on a phone or tablet without one these
// controls stand in for it:
//   - a small "PAD" button at the top right shows or hides them. It shows
//     when the app opens and whenever it is pressed, then fades out within a
//     few seconds, but keeps working where it was;
//   - when shown, a pad (8 directions) at the bottom left, A and B at the
//     bottom right, and START and MENU (Select: the Plus menu) at the bottom
//     centre. They press the same keys a controller would, for every game
//     and for the Plus menu (ApplyTouchControls(), called by ProcessInput());
//   - their opacity is a setting (Plus menu > Settings); at 0% they still
//     work, unseen.
// Whether they are shown and their opacity are kept in Plus.ini. Touches are
// left in touchInfo for the games too (the earlier releases' mobile scripts
// have their own touch controls).
//
// Coordinates are the game's screen pixels (touchInfo holds fractions of it).

namespace RSDK
{
namespace Plus
{

/** How long the PAD button shows after the app opens or it is pressed, and its fade, in frames. */
const int32 kToggleShownFrames = 4 * 60;
const int32 kToggleFadeFrames  = 30;
/** The PAD button's opacity while it shows (out of 0xFF). */
const int32 kToggleAlpha = 0xC0;

/** Colours of the controls. */
const uint32 kTouchBaseColor    = 0x303040;
const uint32 kTouchButtonColor  = 0x585870;
const uint32 kTouchPressedColor = 0xA8A8C8;
const uint32 kTouchLabelColor   = 0xF0F0F0;

/** A circle on screen. */
struct TouchCircle {
    int32 x = 0, y = 0, r = 0;

    /** Whether (px, py) is within @p slack times the radius (touches are imprecise). */
    bool Contains(int32 px, int32 py, int32 slackPercent) const
    {
        const int64 dx    = px - x;
        const int64 dy    = py - y;
        const int64 reach = (int64)r * slackPercent / 100;
        return dx * dx + dy * dy <= reach * reach;
    }
};

/** A box on screen. */
struct TouchBox {
    int32 x = 0, y = 0, w = 0, h = 0;

    /** Whether (px, py) is within the box grown by @p margin on every side. */
    bool Contains(int32 px, int32 py, int32 margin) const
    {
        return px >= x - margin && px < x + w + margin && py >= y - margin && py < y + h + margin;
    }
};

/** Where the controls are, for the current screen size. */
struct TouchLayout {
    TouchBox toggle;
    TouchCircle pad;
    TouchCircle a, b;
    TouchBox start, select;
};

/** Which controls are held this frame. */
struct TouchKeys {
    bool up = false, down = false, left = false, right = false;
    bool a = false, b = false, start = false, select = false;
};

/** The touch controls' state between frames. */
struct TouchState {
    int32 toggleFrames = kToggleShownFrames; ///< Frames the PAD button still shows (then fades).
    bool toggleHeld    = false;              ///< A touch was on it last frame (holding toggles once).
    TouchKeys held;                          ///< For drawing them pressed.
};

TouchState touch;

/** The controls' layout on screen 0, scaled with its height (240 pixels: a 40-pixel pad). */
TouchLayout CurrentTouchLayout()
{
    const int32 w       = screens[0].size.x;
    const int32 h       = screens[0].size.y;
    const int32 padR    = h / 6;
    const int32 buttonR = h / 12;
    const int32 edge    = 14;

    TouchLayout layout;
    layout.toggle = { w - 34, 6, 28, 16 };
    layout.pad    = { edge + padR, h - edge - padR, padR };
    layout.a      = { w - edge - buttonR, h - edge - buttonR * 3, buttonR };
    layout.b      = { w - edge - buttonR * 3 - 6, h - edge - buttonR, buttonR };
    layout.start  = { w / 2 + 6, h - 20, 44, 14 };
    layout.select = { w / 2 - 50, h - 20, 44, 14 };
    return layout;
}

/** The pad's directions for a touch at (dx, dy) from its centre: 8 directions, with a small dead zone. */
void PadDirections(int32 dx, int32 dy, int32 radius, TouchKeys *keys)
{
    const int32 dead = radius / 4;
    const int32 ax   = dx < 0 ? -dx : dx;
    const int32 ay   = dy < 0 ? -dy : dy;
    if (ax <= dead && ay <= dead)
        return;
    // A direction counts when it is within 67.5 degrees of it (tan 22.5 ~ 0.414): diagonals press two.
    if (ax * 1000 > ay * 414) {
        keys->left  = dx < 0;
        keys->right = dx > 0;
    }
    if (ay * 1000 > ax * 414) {
        keys->up   = dy < 0;
        keys->down = dy > 0;
    }
}

/** Presses @p keys on controller @p id, as an input device would (ProcessInput() makes press and down of them). */
void PressTouchKeys(const TouchKeys &keys, int32 id)
{
    ControllerState &pad = controller[id];
    pad.keyUp.press |= keys.up;
    pad.keyDown.press |= keys.down;
    pad.keyLeft.press |= keys.left;
    pad.keyRight.press |= keys.right;
    pad.keyA.press |= keys.a;
    pad.keyB.press |= keys.b;
    pad.keyStart.press |= keys.start;
    pad.keySelect.press |= keys.select;
}

/** The alpha and ink for drawing at @p alpha (out of 0xFF): fully opaque draws without blending. */
int32 InkFor(int32 alpha) { return alpha >= 0xFF ? INK_NONE : INK_ALPHA; }

/** The pad: a disc with a cross, the held directions lit. */
void DrawTouchPad(const TouchCircle &pad, const TouchKeys &keys, int32 alpha)
{
    const int32 ink  = InkFor(alpha);
    const int32 arm  = pad.r * 2 / 3;        // The cross's thickness.
    const int32 half = pad.r * 4 / 5;        // How far its arms reach.
    const int32 tip  = half - arm / 2;       // An arm's length beyond the centre square.
    DrawCircle(pad.x, pad.y, pad.r, kTouchBaseColor, alpha, ink, true);
    DrawRectangle(pad.x - half, pad.y - arm / 2, half * 2, arm, kTouchButtonColor, alpha, ink, true);
    DrawRectangle(pad.x - arm / 2, pad.y - half, arm, half * 2, kTouchButtonColor, alpha, ink, true);
    if (keys.up)
        DrawRectangle(pad.x - arm / 2, pad.y - half, arm, tip, kTouchPressedColor, alpha, ink, true);
    if (keys.down)
        DrawRectangle(pad.x - arm / 2, pad.y + arm / 2, arm, tip, kTouchPressedColor, alpha, ink, true);
    if (keys.left)
        DrawRectangle(pad.x - half, pad.y - arm / 2, tip, arm, kTouchPressedColor, alpha, ink, true);
    if (keys.right)
        DrawRectangle(pad.x + arm / 2, pad.y - arm / 2, tip, arm, kTouchPressedColor, alpha, ink, true);
}

/** A round button, with its label when the controls are at least half opaque. */
void DrawTouchButton(const TouchCircle &button, const char *label, bool held, int32 alpha)
{
    DrawCircle(button.x, button.y, button.r, held ? kTouchPressedColor : kTouchButtonColor, alpha, InkFor(alpha), true);
    if (alpha >= 0x80)
        DrawDevString(label, button.x, button.y - 4, ALIGN_CENTER, kTouchLabelColor);
}

/** A small rectangular button, with its label when the controls are at least half opaque. */
void DrawTouchBox(const TouchBox &box, const char *label, bool held, int32 alpha)
{
    DrawRectangle(box.x, box.y, box.w, box.h, held ? kTouchPressedColor : kTouchButtonColor, alpha, InkFor(alpha), true);
    if (alpha >= 0x80)
        DrawDevString(label, box.x + box.w / 2, box.y + (box.h - 8) / 2, ALIGN_CENTER, kTouchLabelColor);
}

/** Draws the PAD button (while it shows) and, when shown, the controls. Called by DrawOverlay(). */
void DrawTouchControls()
{
    if (!TouchControlsAvailable())
        return;

    // Over the whole of screen 0, whatever the game left the clipping at.
    currentScreen       = &screens[0];
    const int32 clip[4] = { currentScreen->clipBound_X1, currentScreen->clipBound_X2, currentScreen->clipBound_Y1, currentScreen->clipBound_Y2 };
    currentScreen->clipBound_X1 = 0;
    currentScreen->clipBound_X2 = currentScreen->size.x;
    currentScreen->clipBound_Y1 = 0;
    currentScreen->clipBound_Y2 = currentScreen->size.y;

    const TouchLayout layout = CurrentTouchLayout();
    if (TouchControlsShown() && TouchOpacity() > 0) {
        const int32 alpha = TouchOpacity() * 0xFF / 100;
        DrawTouchPad(layout.pad, touch.held, alpha);
        DrawTouchButton(layout.a, "A", touch.held.a, alpha);
        DrawTouchButton(layout.b, "B", touch.held.b, alpha);
        DrawTouchBox(layout.start, "START", touch.held.start, alpha);
        DrawTouchBox(layout.select, "MENU", touch.held.select, alpha);
    }

    if (touch.toggleFrames > 0) {
        const int32 alpha = touch.toggleFrames >= kToggleFadeFrames ? kToggleAlpha : kToggleAlpha * touch.toggleFrames / kToggleFadeFrames;
        const TouchBox &box = layout.toggle;
        DrawRectangle(box.x, box.y, box.w, box.h, touch.toggleHeld ? kTouchPressedColor : kTouchBaseColor, alpha, INK_ALPHA, true);
        if (alpha >= 0x60)
            DrawDevString("PAD", box.x + box.w / 2, box.y + 4, ALIGN_CENTER, TouchControlsShown() ? kGoodColor : kTouchLabelColor);
    }

    currentScreen->clipBound_X1 = clip[0];
    currentScreen->clipBound_X2 = clip[1];
    currentScreen->clipBound_Y1 = clip[2];
    currentScreen->clipBound_Y2 = clip[3];
}

} // namespace Plus
} // namespace RSDK

void RSDK::Plus::ApplyTouchControls()
{
    if (!TouchControlsAvailable())
        return;

    const TouchLayout layout = CurrentTouchLayout();
    const int32 w            = screens[0].size.x;
    const int32 h            = screens[0].size.y;
    const bool shown         = TouchControlsShown();

    TouchKeys keys;
    bool onToggle = false;
    for (int32 t = 0; t < touchInfo.count; ++t) {
        if (!touchInfo.down[t])
            continue;
        const int32 x = (int32)(touchInfo.x[t] * w);
        const int32 y = (int32)(touchInfo.y[t] * h);

        if (layout.toggle.Contains(x, y, 6)) {
            onToggle = true;
            continue;
        }
        if (!shown)
            continue;
        if (layout.pad.Contains(x, y, 150))
            PadDirections(x - layout.pad.x, y - layout.pad.y, layout.pad.r, &keys);
        else if (layout.a.Contains(x, y, 135))
            keys.a = true;
        else if (layout.b.Contains(x, y, 135))
            keys.b = true;
        else if (layout.start.Contains(x, y, 6))
            keys.start = true;
        else if (layout.select.Contains(x, y, 6))
            keys.select = true;
    }

    // The PAD button toggles once per touch, and shows again for a while.
    if (onToggle && !touch.toggleHeld) {
        SetTouchControlsShown(!shown);
        touch.toggleFrames = kToggleShownFrames;
    }
    else if (touch.toggleFrames > 0 && !onToggle) {
        --touch.toggleFrames;
    }
    touch.toggleHeld = onToggle;
    touch.held       = keys;

    // Player 1 (the retro games' player, and Sonic Mania's) and "any controller" (menus).
    PressTouchKeys(keys, CONT_ANY);
    PressTouchKeys(keys, CONT_P1);
}
