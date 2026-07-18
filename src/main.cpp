// Android entry point for VoxelCraft.
// Uses NativeActivity — receives lifecycle and input events through its callbacks.

#include "game/Game.h"
#include "game/InputManager.h"
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <memory>
#include <string>
#include <cstring>
#include <chrono>

#define LOG_TAG "VoxelCraft"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Globals used by Game to load assets and find a writable save directory.
AAssetManager* g_assetManager = nullptr;
std::string g_filesDir;

namespace vc {
static void decodeMotionEvent(AInputEvent* event, int* outAction, int* outPointerId,
                              float* outX, float* outY, int64_t* outTime)
{
    int32_t action = AMotionEvent_getAction(event);
    int masked = action & AMOTION_EVENT_ACTION_MASK;
    int index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    *outAction = masked;
    *outPointerId = AMotionEvent_getPointerId(event, index);
    *outX = AMotionEvent_getX(event, index);
    *outY = AMotionEvent_getY(event, index);
    *outTime = (int64_t)AMotionEvent_getEventTime(event);
}
} // namespace vc

static void onAppCmd(struct android_app* app, int32_t cmd) {
    vc::Game* game = (vc::Game*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            if (game) game->onPause();
            break;
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr && game) {
                int32_t w = ANativeWindow_getWidth(app->window);
                int32_t h = ANativeWindow_getHeight(app->window);
                game->onWindowCreated(app->window, w, h);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            if (game) game->onWindowDestroyed();
            break;
        case APP_CMD_RESUME:
            if (game) game->onResume();
            break;
        case APP_CMD_PAUSE:
            if (game) game->onPause();
            break;
        case APP_CMD_CONFIG_CHANGED:
            if (app->window && game) {
                int32_t w = ANativeWindow_getWidth(app->window);
                int32_t h = ANativeWindow_getHeight(app->window);
                game->onWindowResized(w, h);
            }
            break;
        default:
            break;
    }
}

static int32_t onInputEvent(struct android_app* app, AInputEvent* event) {
    vc::Game* game = (vc::Game*)app->userData;
    if (!game) return 0;
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        int action, pointerId;
        float x, y;
        int64_t time;
        vc::decodeMotionEvent(event, &action, &pointerId, &x, &y, &time);
        game->onMotionEvent(action, pointerId, x, y, time);
        return 1;
    } else if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t code = AKeyEvent_getKeyCode(event);
        int32_t a    = AKeyEvent_getAction(event);
        game->onKeyEvent(code, a == AKEY_EVENT_ACTION_DOWN);
        return 1;
    }
    return 0;
}

static std::string getFilesDir(struct android_app* app) {
    if (app->activity->internalDataPath) {
        return std::string(app->activity->internalDataPath);
    }
    return std::string("/data/data/com.voxelcraft/files");
}

extern "C" {
void android_main(struct android_app* app) {
    // Esto es necesario para que native_app_glue procese los eventos correctamente
    app_dummy();

    LOGI("VoxelCraft starting up...");

    g_assetManager = app->activity->assetManager;
    g_filesDir = getFilesDir(app);

    vc::Game game;
    app->userData = &game;
    app->onAppCmd = onAppCmd;
    app->onInputEvent = onInputEvent;

    while (!app->destroyRequested) {
        int events;
        struct android_poll_source* source;
        int timeout = (app->window != nullptr) ? 0 : -1;
        
        while (ALooper_pollAll(timeout, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested != 0) break;
        }
        
        if (app->window != nullptr) {
            game.tick();
        }
    }
    LOGI("VoxelCraft shutting down...");
}
}

