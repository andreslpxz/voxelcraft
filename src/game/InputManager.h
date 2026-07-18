#pragma once
#include <android/input.h>
#include <android/native_window.h>
#include <cstdint>
#include <vector>

namespace vc {

// Touch zones — left half of screen = movement joystick (virtual),
// right half = look (camera drag).
// Tap on hotbar = select slot.
// Single tap on right side while not dragging = break block.
// Long-press on right side = place block.

struct InputState {
    // Look drag (right side)
    int32_t lookPointerId = -1;
    float   lookLastX = 0;
    float   lookLastY = 0;
    float   lookDX = 0;
    float   lookDY = 0;
    bool    lookDragging = false;
    bool    lookTapped = false;     // single tap on right side (becomes break)
    int64_t lookDownTime = 0;
    float   lookStartX = 0, lookStartY = 0;

    // Joystick (left side)
    int32_t movePointerId = -1;
    float   moveOriginX = 0, moveOriginY = 0;
    float   moveDX = 0, moveDY = 0;
    bool    moveActive = false;

    // Hotbar tap (anywhere on hotbar area)
    int     hotbarSelected = -1;

    // Jump / fly buttons (top-right corner)
    bool    jumpPressed = false;
    bool    jumpHeld = false;

    int     screenWidth  = 0;
    int     screenHeight = 0;

    void resetFrame() {
        lookDX = 0; lookDY = 0;
        lookTapped = false;
        hotbarSelected = -1;
        jumpPressed = false;
    }
};

class InputManager {
public:
    // Process an Android MotionEvent-style input event
    // action: AMOTION_EVENT_ACTION_DOWN / MOVE / UP
    // pointerId: the pointer identifier
    // x, y: screen coordinates (pixels)
    void onMotion(int action, int pointerId, float x, float y, int64_t timeMs);

    // Process a key event (back button, etc.)
    void onKey(int keyCode, bool down);

    void setScreenSize(int w, int h) {
        state_.screenWidth = w;
        state_.screenHeight = h;
    }

    const InputState& state() const { return state_; }
    InputState& state() { return state_; }

private:
    InputState state_;

    bool isLeftHalf(float x) const {
        return x < state_.screenWidth * 0.5f;
    }
    bool isHotbar(float y) const {
        return y > state_.screenHeight - 120.0f;
    }
};

} // namespace vc
