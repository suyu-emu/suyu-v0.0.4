// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.annotation.SuppressLint
import android.app.AlertDialog
import android.content.Context
import android.content.DialogInterface
import android.content.SharedPreferences
import android.content.pm.ActivityInfo
import android.content.res.Resources
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.*
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.widget.PopupMenu
import androidx.core.content.res.ResourcesCompat
import androidx.core.graphics.Insets
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.preference.PreferenceManager
import androidx.window.layout.FoldingFeature
import androidx.window.layout.WindowLayoutInfo
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.databinding.DialogOverlayAdjustBinding
import org.yuzu.yuzu_emu.databinding.FragmentEmulationBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.*
import org.yuzu.yuzu_emu.utils.SerializableHelper.parcelable

class EmulationFragment : Fragment(), SurfaceHolder.Callback {
    private lateinit var preferences: SharedPreferences
    private lateinit var emulationState: EmulationState
    private var emulationActivity: EmulationActivity? = null
    private var perfStatsUpdater: (() -> Unit)? = null

    private var _binding: FragmentEmulationBinding? = null
    private val binding get() = _binding!!

    private lateinit var game: Game

    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is EmulationActivity) {
            emulationActivity = context
            NativeLibrary.setEmulationActivity(context)
        } else {
            throw IllegalStateException("EmulationFragment must have EmulationActivity parent")
        }
    }

    /**
     * Initialize anything that doesn't depend on the layout / views in here.
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // So this fragment doesn't restart on configuration changes; i.e. rotation.
        retainInstance = true
        preferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        game = requireArguments().parcelable(EmulationActivity.EXTRA_SELECTED_GAME)!!
        emulationState = EmulationState(game.path)
    }

    /**
     * Initialize the UI and start emulation in here.
     */
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentEmulationBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.surfaceEmulation.holder.addCallback(this)
        binding.showFpsText.setTextColor(Color.YELLOW)
        binding.doneControlConfig.setOnClickListener { stopConfiguringControls() }

        // Setup overlay.
        updateShowFpsOverlay()

        binding.inGameMenu.getHeaderView(0).findViewById<TextView>(R.id.text_game_title).text =
            game.title
        binding.inGameMenu.setNavigationItemSelectedListener {
            when (it.itemId) {
                R.id.menu_pause_emulation -> {
                    if (emulationState.isPaused) {
                        emulationState.run(false)
                        it.title = resources.getString(R.string.emulation_pause)
                        it.icon = ResourcesCompat.getDrawable(
                            resources,
                            R.drawable.ic_pause,
                            requireContext().theme
                        )
                    } else {
                        emulationState.pause()
                        it.title = resources.getString(R.string.emulation_unpause)
                        it.icon = ResourcesCompat.getDrawable(
                            resources,
                            R.drawable.ic_play,
                            requireContext().theme
                        )
                    }
                    true
                }

                R.id.menu_settings -> {
                    SettingsActivity.launch(requireContext(), SettingsFile.FILE_NAME_CONFIG, "")
                    true
                }

                R.id.menu_overlay_controls -> {
                    showOverlayOptions()
                    true
                }

                R.id.menu_exit -> {
                    emulationState.stop()
                    requireActivity().finish()
                    true
                }

                else -> true
            }
        }

        setInsets()

        requireActivity().onBackPressedDispatcher.addCallback(
            requireActivity(),
            object : OnBackPressedCallback(true) {
                override fun handleOnBackPressed() {
                    if (binding.drawerLayout.isOpen) binding.drawerLayout.close() else binding.drawerLayout.open()
                }
            })
    }

    override fun onResume() {
        super.onResume()
        if (!DirectoryInitialization.areDirectoriesReady) {
            DirectoryInitialization.start(requireContext())
        }
        emulationState.run(emulationActivity!!.isActivityRecreated)
    }

    override fun onPause() {
        if (emulationState.isRunning) {
            emulationState.pause()
        }
        super.onPause()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onDetach() {
        NativeLibrary.clearEmulationActivity()
        super.onDetach()
    }

    private fun refreshInputOverlay() {
        binding.surfaceInputOverlay.refreshControls()
    }

    private fun resetInputOverlay() {
        preferences.edit()
            .remove(Settings.PREF_CONTROL_SCALE)
            .remove(Settings.PREF_CONTROL_OPACITY)
            .apply()
        binding.surfaceInputOverlay.post { binding.surfaceInputOverlay.resetButtonPlacement() }
    }

    private fun updateShowFpsOverlay() {
        if (EmulationMenuSettings.showFps) {
            val SYSTEM_FPS = 0
            val FPS = 1
            val FRAMETIME = 2
            val SPEED = 3
            perfStatsUpdater = {
                val perfStats = NativeLibrary.getPerfStats()
                if (perfStats[FPS] > 0 && _binding != null) {
                    binding.showFpsText.text = String.format("FPS: %.1f", perfStats[FPS])
                }

                if (!emulationState.isStopped) {
                    perfStatsUpdateHandler.postDelayed(perfStatsUpdater!!, 100)
                }
            }
            perfStatsUpdateHandler.post(perfStatsUpdater!!)
            binding.showFpsText.text = resources.getString(R.string.emulation_game_loading)
            binding.showFpsText.visibility = View.VISIBLE
        } else {
            if (perfStatsUpdater != null) {
                perfStatsUpdateHandler.removeCallbacks(perfStatsUpdater!!)
            }
            binding.showFpsText.visibility = View.GONE
        }
    }

    private val Number.toPx get() = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, this.toFloat(), Resources.getSystem().displayMetrics).toInt()

    fun updateCurrentLayout(emulationActivity: EmulationActivity, newLayoutInfo: WindowLayoutInfo) {
        val isFolding = (newLayoutInfo.displayFeatures.find { it is FoldingFeature } as? FoldingFeature)?.let {
            if (it.isSeparating) {
                emulationActivity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
                if (it.orientation == FoldingFeature.Orientation.HORIZONTAL) {
                    binding.surfaceEmulation.layoutParams.height = it.bounds.top
                    binding.inGameMenu.layoutParams.height = it.bounds.bottom
                    binding.overlayContainer.layoutParams.height = it.bounds.bottom - 48.toPx
                    binding.overlayContainer.updatePadding(0, 0, 0, 24.toPx)
                }
            }
            it.isSeparating
        } ?: false
        if (!isFolding) {
            binding.surfaceEmulation.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            binding.inGameMenu.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            binding.overlayContainer.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            binding.overlayContainer.updatePadding(0, 0, 0, 0)
            emulationActivity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        }
        binding.surfaceInputOverlay.requestLayout()
        binding.inGameMenu.requestLayout()
        binding.overlayContainer.requestLayout()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        // We purposely don't do anything here.
        // All work is done in surfaceChanged, which we are guaranteed to get even for surface creation.
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.debug("[EmulationFragment] Surface changed. Resolution: " + width + "x" + height)
        emulationState.newSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        emulationState.clearSurface()
    }

    private fun showOverlayOptions() {
        val anchor = binding.inGameMenu.findViewById<View>(R.id.menu_overlay_controls)
        val popup = PopupMenu(requireContext(), anchor)

        popup.menuInflater.inflate(R.menu.menu_overlay_options, popup.menu)

        popup.menu.apply {
            findItem(R.id.menu_toggle_fps).isChecked = EmulationMenuSettings.showFps
            findItem(R.id.menu_rel_stick_center).isChecked = EmulationMenuSettings.joystickRelCenter
            findItem(R.id.menu_dpad_slide).isChecked = EmulationMenuSettings.dpadSlide
            findItem(R.id.menu_show_overlay).isChecked = EmulationMenuSettings.showOverlay
            findItem(R.id.menu_haptics).isChecked = EmulationMenuSettings.hapticFeedback
        }

        popup.setOnMenuItemClickListener {
            when (it.itemId) {
                R.id.menu_toggle_fps -> {
                    it.isChecked = !it.isChecked
                    EmulationMenuSettings.showFps = it.isChecked
                    updateShowFpsOverlay()
                    true
                }

                R.id.menu_edit_overlay -> {
                    binding.drawerLayout.close()
                    binding.surfaceInputOverlay.requestFocus()
                    startConfiguringControls()
                    true
                }

                R.id.menu_adjust_overlay -> {
                    adjustOverlay()
                    true
                }

                R.id.menu_toggle_controls -> {
                    val preferences =
                        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
                    val optionsArray = BooleanArray(15)
                    for (i in 0..14) {
                        optionsArray[i] = preferences.getBoolean("buttonToggle$i", i < 13)
                    }

                    val dialog = MaterialAlertDialogBuilder(requireContext())
                        .setTitle(R.string.emulation_toggle_controls)
                        .setMultiChoiceItems(
                            R.array.gamepadButtons,
                            optionsArray
                        ) { _, indexSelected, isChecked ->
                            preferences.edit()
                                .putBoolean("buttonToggle$indexSelected", isChecked)
                                .apply()
                        }
                        .setPositiveButton(android.R.string.ok) { _, _ ->
                            refreshInputOverlay()
                        }
                        .setNegativeButton(android.R.string.cancel, null)
                        .setNeutralButton(R.string.emulation_toggle_all) { _, _ -> }
                        .show()

                    // Override normal behaviour so the dialog doesn't close
                    dialog.getButton(AlertDialog.BUTTON_NEUTRAL)
                        .setOnClickListener {
                            val isChecked = !optionsArray[0]
                            for (i in 0..14) {
                                optionsArray[i] = isChecked
                                dialog.listView.setItemChecked(i, isChecked)
                                preferences.edit()
                                    .putBoolean("buttonToggle$i", isChecked)
                                    .apply()
                            }
                        }
                    true
                }

                R.id.menu_show_overlay -> {
                    it.isChecked = !it.isChecked
                    EmulationMenuSettings.showOverlay = it.isChecked
                    refreshInputOverlay()
                    true
                }

                R.id.menu_rel_stick_center -> {
                    it.isChecked = !it.isChecked
                    EmulationMenuSettings.joystickRelCenter = it.isChecked
                    true
                }

                R.id.menu_dpad_slide -> {
                    it.isChecked = !it.isChecked
                    EmulationMenuSettings.dpadSlide = it.isChecked
                    true
                }

                R.id.menu_haptics -> {
                    it.isChecked = !it.isChecked
                    EmulationMenuSettings.hapticFeedback = it.isChecked
                    true
                }

                R.id.menu_reset_overlay -> {
                    binding.drawerLayout.close()
                    resetInputOverlay()
                    true
                }

                else -> true
            }
        }

        popup.show()
    }

    private fun startConfiguringControls() {
        binding.doneControlConfig.visibility = View.VISIBLE
        binding.surfaceInputOverlay.setIsInEditMode(true)
    }

    private fun stopConfiguringControls() {
        binding.doneControlConfig.visibility = View.GONE
        binding.surfaceInputOverlay.setIsInEditMode(false)
    }

    @SuppressLint("SetTextI18n")
    private fun adjustOverlay() {
        val adjustBinding = DialogOverlayAdjustBinding.inflate(layoutInflater)
        adjustBinding.apply {
            inputScaleSlider.apply {
                valueTo = 150F
                value = preferences.getInt(Settings.PREF_CONTROL_SCALE, 50).toFloat()
                addOnChangeListener(Slider.OnChangeListener { _, value, _ ->
                    inputScaleValue.text = "${value.toInt()}%"
                    setControlScale(value.toInt())
                })
            }
            inputOpacitySlider.apply {
                valueTo = 100F
                value = preferences.getInt(Settings.PREF_CONTROL_OPACITY, 100).toFloat()
                addOnChangeListener(Slider.OnChangeListener { _, value, _ ->
                    inputOpacityValue.text = "${value.toInt()}%"
                    setControlOpacity(value.toInt())
                })
            }
            inputScaleValue.text = "${inputScaleSlider.value.toInt()}%"
            inputOpacityValue.text = "${inputOpacitySlider.value.toInt()}%"
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.emulation_control_adjust)
            .setView(adjustBinding.root)
            .setPositiveButton(android.R.string.ok, null)
            .setNeutralButton(R.string.slider_default) { _: DialogInterface?, _: Int ->
                setControlScale(50)
                setControlOpacity(100)
            }
            .show()
    }

    private fun setControlScale(scale: Int) {
        preferences.edit()
            .putInt(Settings.PREF_CONTROL_SCALE, scale)
            .apply()
        refreshInputOverlay()
    }

    private fun setControlOpacity(opacity: Int) {
        preferences.edit()
            .putInt(Settings.PREF_CONTROL_OPACITY, opacity)
            .apply()
        refreshInputOverlay()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.inGameMenu) { v: View, windowInsets: WindowInsetsCompat ->
            val cutInsets: Insets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            var left = 0
            var right = 0
            if (ViewCompat.getLayoutDirection(v) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                left = cutInsets.left
            } else {
                right = cutInsets.right
            }

            v.setPadding(left, cutInsets.top, right, 0)

            // Ensure FPS text doesn't get cut off by rounded display corners
            val sidePadding = resources.getDimensionPixelSize(R.dimen.spacing_xtralarge)
            if (cutInsets.left == 0) {
                binding.showFpsText.setPadding(
                    sidePadding,
                    cutInsets.top,
                    cutInsets.right,
                    cutInsets.bottom
                )
            } else {
                binding.showFpsText.setPadding(
                    cutInsets.left,
                    cutInsets.top,
                    cutInsets.right,
                    cutInsets.bottom
                )
            }
            windowInsets
        }
    }

    private class EmulationState(private val gamePath: String) {
        private var state: State
        private var surface: Surface? = null
        private var runWhenSurfaceIsValid = false

        init {
            // Starting state is stopped.
            state = State.STOPPED
        }

        @get:Synchronized
        val isStopped: Boolean
            get() = state == State.STOPPED

        // Getters for the current state
        @get:Synchronized
        val isPaused: Boolean
            get() = state == State.PAUSED

        @get:Synchronized
        val isRunning: Boolean
            get() = state == State.RUNNING

        @Synchronized
        fun stop() {
            if (state != State.STOPPED) {
                Log.debug("[EmulationFragment] Stopping emulation.")
                NativeLibrary.stopEmulation()
                state = State.STOPPED
            } else {
                Log.warning("[EmulationFragment] Stop called while already stopped.")
            }
        }

        // State changing methods
        @Synchronized
        fun pause() {
            if (state != State.PAUSED) {
                Log.debug("[EmulationFragment] Pausing emulation.")

                NativeLibrary.pauseEmulation()

                state = State.PAUSED
            } else {
                Log.warning("[EmulationFragment] Pause called while already paused.")
            }
        }

        @Synchronized
        fun run(isActivityRecreated: Boolean) {
            if (isActivityRecreated) {
                if (NativeLibrary.isRunning()) {
                    state = State.PAUSED
                }
            } else {
                Log.debug("[EmulationFragment] activity resumed or fresh start")
            }

            // If the surface is set, run now. Otherwise, wait for it to get set.
            if (surface != null) {
                runWithValidSurface()
            } else {
                runWhenSurfaceIsValid = true
            }
        }

        // Surface callbacks
        @Synchronized
        fun newSurface(surface: Surface?) {
            this.surface = surface
            if (runWhenSurfaceIsValid) {
                runWithValidSurface()
            }
        }

        @Synchronized
        fun clearSurface() {
            if (surface == null) {
                Log.warning("[EmulationFragment] clearSurface called, but surface already null.")
            } else {
                surface = null
                Log.debug("[EmulationFragment] Surface destroyed.")
                when (state) {
                    State.RUNNING -> {
                        state = State.PAUSED
                    }

                    State.PAUSED -> Log.warning("[EmulationFragment] Surface cleared while emulation paused.")
                    else -> Log.warning("[EmulationFragment] Surface cleared while emulation stopped.")
                }
            }
        }

        private fun runWithValidSurface() {
            runWhenSurfaceIsValid = false
            when (state) {
                State.STOPPED -> {
                    NativeLibrary.surfaceChanged(surface)
                    val emulationThread = Thread({
                        Log.debug("[EmulationFragment] Starting emulation thread.")
                        NativeLibrary.run(gamePath)
                    }, "NativeEmulation")
                    emulationThread.start()
                }

                State.PAUSED -> {
                    Log.debug("[EmulationFragment] Resuming emulation.")
                    NativeLibrary.surfaceChanged(surface)
                    NativeLibrary.unPauseEmulation()
                }

                else -> Log.debug("[EmulationFragment] Bug, run called while already running.")
            }
            state = State.RUNNING
        }

        private enum class State {
            STOPPED, RUNNING, PAUSED
        }
    }

    companion object {
        private val perfStatsUpdateHandler = Handler(Looper.myLooper()!!)

        fun newInstance(game: Game): EmulationFragment {
            val args = Bundle()
            args.putParcelable(EmulationActivity.EXTRA_SELECTED_GAME, game)
            val fragment = EmulationFragment()
            fragment.arguments = args
            return fragment
        }
    }
}
