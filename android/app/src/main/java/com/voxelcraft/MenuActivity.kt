package com.voxelcraft

import android.app.Activity
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.ViewAnimationUtils
import android.view.WindowManager
import android.view.animation.AccelerateDecelerateInterpolator
import android.view.animation.OvershootInterpolator
import android.widget.Button
import android.widget.SeekBar
import android.widget.TextView
import android.widget.FrameLayout
import kotlin.math.hypot

/**
 * Pantalla principal de VoxelCraft.
 *
 * Sustituye al launcher anterior (que arrancaba directamente el NativeActivity
 * nativo de Vulkan) por una pantalla de menú visualmente cuidada:
 *
 *   - Fondo en gradiente cielo → hierba con cubos decorativos.
 *   - Logo "VoxelCraft" + tagline + separador.
 *   - Botón grande "JUGAR" + secundarios (CONTINUAR, CONFIGURACIÓN, ACERCA DE, SALIR).
 *   - Diálogos modales para Configuración y Acerca de.
 *
 * Al pulsar JUGAR se lanza [GameActivity], que es la actividad nativa
 * (NativeActivity) con la lib `voxelcraft` cargada.
 *
 * Las preferencias (distancia de render, sensibilidad) se guardan en
 * SharedPreferences y se pasan al motor nativo vía Intent extras; el
 * NativeActivity puede leerlas desde C++ a través de `android_app->activity`.
 */
class MenuActivity : Activity() {

    // Vistas raíz de los diálogos (null cuando están cerrados)
    private var settingsOverlay: View? = null
    private var aboutOverlay: View? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Pantalla completa, a prueba de notch, sin timeout de pantalla.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.attributes.layoutInDisplayCutoutMode =
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        window.setFlags(
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
            WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS
        )

        setContentView(R.layout.activity_menu)

        wireButtons()
        playEntranceAnimation()
    }

    /**
     * Conecta los listeners de todos los botones del menú.
     */
    private fun wireButtons() {
        findViewById<Button>(R.id.btn_play).setOnClickListener {
            launchGame(newWorld = false)
        }
        findViewById<Button>(R.id.btn_continue).setOnClickListener {
            launchGame(newWorld = false)
        }
        findViewById<Button>(R.id.btn_settings).setOnClickListener {
            showSettingsDialog()
        }
        findViewById<Button>(R.id.btn_about).setOnClickListener {
            showAboutDialog()
        }
        findViewById<Button>(R.id.btn_quit).setOnClickListener {
            finishAffinity()
        }
    }

    /**
     * Lanza el juego (NativeActivity) con un Intent explícito.
     *
     * Pasa las preferencias como extras para que el motor C++ las lea
     * vía `android_app->activity->intent` (ver main.cpp).
     */
    private fun launchGame(newWorld: Boolean) {
        val prefs = getSharedPreferences("voxelcraft_prefs", MODE_PRIVATE)
        val intent = Intent(this, GameActivity::class.java).apply {
            addFlags(Intent.FLAG_ACTIVITY_NO_HISTORY)
            putExtra(EXTRA_RENDER_DISTANCE, prefs.getInt(PREF_RENDER_DISTANCE, 6))
            putExtra(EXTRA_SENSITIVITY,     prefs.getInt(PREF_SENSITIVITY, 50))
            putExtra(EXTRA_NEW_WORLD,       newWorld)
        }
        startActivity(intent)
        overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
    }

    // ----------------------------------------------------------
    // Animación de entrada: logo + botones aparecen en secuencia.
    // ----------------------------------------------------------
    private fun playEntranceAnimation() {
        val rootView = findViewById<FrameLayout>(android.R.id.content)
        // Buscamos el contenedor del logo y el de los botones (según el layout).
        val logo = rootView.findViewWithTag<View>("logo_block")
        // Animación simple: alpha + translateY para todos los botones.
        val buttons = listOf(
            R.id.btn_play, R.id.btn_continue,
            R.id.btn_settings, R.id.btn_about, R.id.btn_quit
        ).map { findViewById<View>(it) }

        buttons.forEachIndexed { i, v ->
            v.alpha = 0f
            v.translationY = 40f
            v.animate()
                .alpha(1f)
                .translationY(0f)
                .setStartDelay(180L + i * 70L)
                .setDuration(420L)
                .setInterpolator(OvershootInterpolator(0.85f))
                .start()
        }
    }

    // ----------------------------------------------------------
    // Diálogo de Configuración
    // ----------------------------------------------------------
    private fun showSettingsDialog() {
        if (settingsOverlay != null) return // ya está abierto
        val overlay = layoutInflater.inflate(R.layout.dialog_settings, null)
        settingsOverlay = overlay

        val prefs = getSharedPreferences("voxelcraft_prefs", MODE_PRIVATE)
        val initialRender = prefs.getInt(PREF_RENDER_DISTANCE, 6)
        val initialSens   = prefs.getInt(PREF_SENSITIVITY, 50)

        val tvRender = overlay.findViewById<TextView>(R.id.tv_render_distance_value)
        val tvSens   = overlay.findViewById<TextView>(R.id.tv_sensitivity_value)
        val seekRender = overlay.findViewById<SeekBar>(R.id.seek_render_distance)
        val seekSens   = overlay.findViewById<SeekBar>(R.id.seek_sensitivity)

        seekRender.progress = initialRender
        seekSens.progress = initialSens
        tvRender.text = getString(R.string.settings_render_distance_value, initialRender)
        tvSens.text = "${initialSens}%"

        seekRender.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, p: Int, fromUser: Boolean) {
                tvRender.text = getString(R.string.settings_render_distance_value, p)
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {
                prefs.edit().putInt(PREF_RENDER_DISTANCE, sb?.progress ?: 6).apply()
            }
        })

        seekSens.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, p: Int, fromUser: Boolean) {
                tvSens.text = "$p%"
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {
                prefs.edit().putInt(PREF_SENSITIVITY, sb?.progress ?: 50).apply()
            }
        })

        overlay.findViewById<Button>(R.id.btn_settings_close).setOnClickListener {
            dismissDialog(overlay)
        }

        addDialogOverlay(overlay)
    }

    // ----------------------------------------------------------
    // Diálogo de Acerca de
    // ----------------------------------------------------------
    private fun showAboutDialog() {
        if (aboutOverlay != null) return
        val overlay = layoutInflater.inflate(R.layout.dialog_about, null)
        aboutOverlay = overlay
        overlay.findViewById<Button>(R.id.btn_about_close).setOnClickListener {
            dismissDialog(overlay)
        }
        addDialogOverlay(overlay)
    }

    /**
     * Añade un overlay a la ventana raíz y lo anima con un reveal circular.
     */
    private fun addDialogOverlay(view: View) {
        val root = findViewById<FrameLayout>(android.R.id.content)
        root.addView(view)
        view.alpha = 0f
        view.animate().alpha(1f).setDuration(180L).start()
    }

    /**
     * Cierra un overlay con fade y lo retira de la jerarquía.
     */
    private fun dismissDialog(view: View) {
        view.animate()
            .alpha(0f)
            .setDuration(160L)
            .withEndAction {
                (view.parent as? FrameLayout)?.removeView(view)
                if (view === settingsOverlay) settingsOverlay = null
                if (view === aboutOverlay)    aboutOverlay = null
            }
            .start()
    }

    /**
     * Back cierra el diálogo abierto; si no hay ninguno, sale de la app.
     */
    override fun onBackPressed() {
        settingsOverlay?.let { dismissDialog(it); return }
        aboutOverlay?.let    { dismissDialog(it); return }
        super.onBackPressed()
    }

    companion object {
        const val EXTRA_RENDER_DISTANCE = "voxelcraft.render_distance"
        const val EXTRA_SENSITIVITY     = "voxelcraft.sensitivity"
        const val EXTRA_NEW_WORLD       = "voxelcraft.new_world"

        const val PREF_RENDER_DISTANCE = "render_distance"
        const val PREF_SENSITIVITY     = "sensitivity"
    }
}
