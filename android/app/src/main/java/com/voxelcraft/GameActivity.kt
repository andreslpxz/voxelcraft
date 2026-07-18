package com.voxelcraft

import android.app.NativeActivity
import android.os.Bundle
import android.view.WindowManager

/**
 * Wrapper delgado sobre [NativeActivity] para el motor nativo Vulkan de
 * VoxelCraft.
 *
 * Es solo un NativeActivity configurado con la lib `voxelcraft`. Toda la
 * lógica del juego vive en C++ (ver `src/main.cpp`, `src/game/Game.cpp`).
 *
 * El back-button cierra esta actividad y regresa automáticamente a
 * [MenuActivity] gracias a la pila de actividades estándar de Android.
 */
class GameActivity : NativeActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.attributes.layoutInDisplayCutoutMode =
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
    }
}
