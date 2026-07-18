package com.voxelcraft

import android.app.NativeActivity
import android.os.Bundle
import android.view.WindowManager

/**
 * Actividad nativa original de VoxelCraft.
 *
 * Mantiene compatibilidad hacia atrás: si algún launcher externo apunta a
 * `com.voxelcraft.MainActivity`, sigue funcionando. Internamente delega
 * toda la lógica al NativeActivity estándar con la lib `voxelcraft`.
 *
 * El flujo normal hoy es:
 *   MenuActivity  →  JUGAR  →  GameActivity (= NativeActivity)
 *
 * Pero dejamos MainActivity porque el AndroidManifest original la
 * registraba y no queremos romper instalaciones existentes.
 */
class MainActivity : NativeActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.attributes.layoutInDisplayCutoutMode =
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
    }
}
