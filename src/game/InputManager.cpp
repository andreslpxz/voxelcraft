#include "InputManager.h"
#include <cmath>

namespace vc {

// Simplified action constants (mirrors Android MotionEvent values)
enum {
    ACTION_DOWN      = 0,
    ACTION_UP        = 1,
    ACTION_MOVE      = 2,
    ACTION_CANCEL    = 3,
    ACTION_POINTER_DOWN = 5,
    ACTION_POINTER_UP   = 6,
};

void InputManager::onMotion(int action, int pointerId, float x, float y, int64_t timeMs) {
    int masked = action & 0xff;
    switch (masked) {
        case ACTION_DOWN:
        case ACTION_POINTER_DOWN: {
            if (isHotbar(y)) {
                // Hotbar slot selection — split screen into 9 segments
                float slotW = state_.screenWidth / 9.0f;
                state_.hotbarSelected = (int)(x / slotW);
                if (state_.hotbarSelected < 0) state_.hotbarSelected = 0;
                if (state_.hotbarSelected > 8) state_.hotbarSelected = 8;
            } else if (isLeftHalf(x)) {
                if (state_.movePointerId == -1) {
                    state_.movePointerId = pointerId;
                    state_.moveOriginX = x;
                    state_.moveOriginY = y;
                    state_.moveDX = 0;
                    state_.moveDY = 0;
                    state_.moveActive = true;
                }
            } else {
                if (state_.lookPointerId == -1) {
                    state_.lookPointerId = pointerId;
                    state_.lookLastX = x;
                    state_.lookLastY = y;
                    state_.lookStartX = x;
                    state_.lookStartY = y;
                    state_.lookDownTime = timeMs;
                    state_.lookDragging = false;
                }
            }
            break;
        }
        case ACTION_MOVE: {
            if (pointerId == state_.lookPointerId) {
                float dx = x - state_.lookLastX;
                float dy = y - state_.lookLastY;
                state_.lookDX += dx;
                state_.lookDY += dy;
                state_.lookLastX = x;
                state_.lookLastY = y;
                // Become a drag if movement exceeds threshold
                float totalDx = x - state_.lookStartX;
                float totalDy = y - state_.lookStartY;
                if (std::sqrt(totalDx*totalDx + totalDy*totalDy) > 12.0f) {
                    state_.lookDragging = true;
                }
            }
            if (pointerId == state_.movePointerId) {
                float dx = x - state_.moveOriginX;
                float dy = y - state_.moveOriginY;
                // Clamp to a virtual circle of radius 80px
                float r = 80.0f;
                float mag = std::sqrt(dx*dx + dy*dy);
                if (mag > r) { dx = dx * r / mag; dy = dy * r / mag; }
                state_.moveDX = dx / r;
                state_.moveDY = dy / r;
            }
            break;
        }
        case ACTION_UP:
        case ACTION_CANCEL:
        case ACTION_POINTER_UP: {
            if (pointerId == state_.lookPointerId) {
                int64_t dur = timeMs - state_.lookDownTime;
                if (!state_.lookDragging && dur < 250) {
                    state_.lookTapped = true;
                }
                state_.lookPointerId = -1;
                state_.lookDragging = false;
            }
            if (pointerId == state_.movePointerId) {
                state_.movePointerId = -1;
                state_.moveDX = 0;
                state_.moveDY = 0;
                state_.moveActive = false;
            }
            break;
        }
    }
}

void InputManager::onKey(int keyCode, bool down) {
    // Reserved for hardware keys (back, volume, etc.)
}

} // namespace vc
