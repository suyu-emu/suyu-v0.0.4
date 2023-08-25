// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.annotation.SuppressLint
import android.app.AlertDialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Rational
import android.view.*
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.widget.PopupMenu
import androidx.core.content.res.ResourcesCompat
import androidx.core.graphics.Insets
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.fragment.navArgs
import androidx.preference.PreferenceManager
import androidx.window.layout.FoldingFeature
import androidx.window.layout.WindowInfoTracker
import androidx.window.layout.WindowLayoutInfo
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.databinding.DialogOverlayAdjustBinding
import org.yuzu.yuzu_emu.databinding.FragmentEmulationBinding
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.overlay.InputOverlay
import org.yuzu.yuzu_emu.utils.*

class EmulationFragment : Fragment(), SurfaceHolder.Callback {
    private lateinit var preferences: SharedPreferences
    private lateinit var emulationState: EmulationState
    private var emulationActivity: EmulationActivity? = null
    private var perfStatsUpdater: (() -> Unit)? = null

    private var _binding: FragmentEmulationBinding? = null
    private val binding get() = _binding!!

    val args by navArgs<EmulationFragmentArgs>()

    private var isInFoldableLayout = false

    private lateinit var onReturnFromSettings: ActivityResultLauncher<Intent>

    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is EmulationActivity) {
            emulationActivity = context
            NativeLibrary.setEmulationActivity(context)

            lifecycleScope.launch(Dispatchers.Main) {
                lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                    WindowInfoTracker.getOrCreate(context)
                        .windowLayoutInfo(context)
                        .collect { updateFoldableLayout(context, it) }
                }
            }

            onReturnFromSettings = context.activityResultRegistry.register(
                "SettingsResult",
                ActivityResultContracts.StartActivityForResult()
            ) { updateScreenLayout() }
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
        emulationState = EmulationState(args.game.path)
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
            args.game.title
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
                    SettingsActivity.launch(
                        requireContext(),
                        onReturnFromSettings,
                        SettingsFile.FILE_NAME_CONFIG,
                        ""
                    )
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
                    if (binding.drawerLayout.isOpen) {
                        binding.drawerLayout.close()
                    } else {
                        binding.drawerLayout.open()
                    }
                }
            }
        )

        viewLifecycleOwner.lifecycleScope.launch(Dispatchers.Main) {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                WindowInfoTracker.getOrCreate(requireContext())
                    .windowLayoutInfo(requireActivity())
                    .collect { updateFoldableLayout(requireActivity() as EmulationActivity, it) }
            }
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        if (emulationActivity?.isInPictureInPictureMode == true) {
            if (binding.drawerLayout.isOpen) {
                binding.drawerLayout.close()
            }
            if (EmulationMenuSettings.showOverlay) {
                binding.surfaceInputOverlay.post { binding.surfaceInputOverlay.isVisible = false }
            }
        } else {
            if (EmulationMenuSettings.showOverlay) {
                binding.surfaceInputOverlay.post { binding.surfaceInputOverlay.isVisible = true }
            }
            if (!isInFoldableLayout) {
                if (newConfig.orientation == Configuration.ORIENTATION_PORTRAIT) {
                    binding.surfaceInputOverlay.layout = InputOverlay.PORTRAIT
                } else {
                    binding.surfaceInputOverlay.layout = InputOverlay.LANDSCAPE
                }
            }
            if (!binding.surfaceInputOverlay.isInEditMode) {
                refreshInputOverlay()
            }
        }
    }

    override fun onResume() {
        super.onResume()
        if (!DirectoryInitialization.areDirectoriesReady) {
            DirectoryInitialization.start(requireContext())
        }

        updateScreenLayout()

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
        binding.surfaceInputOverlay.post {
            binding.surfaceInputOverlay.resetLayoutVisibilityAndPlacement()
        }
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

    @SuppressLint("SourceLockedOrientationActivity")
    private fun updateOrientation() {
        emulationActivity?.let {
            it.requestedOrientation = when (IntSetting.RENDERER_SCREEN_LAYOUT.int) {
                Settings.LayoutOption_MobileLandscape ->
                    ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
                Settings.LayoutOption_MobilePortrait ->
                    ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
                Settings.LayoutOption_Unspecified -> ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
                else -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            }
        }
    }

    private fun updateScreenLayout() {
        binding.surfaceEmulation.setAspectRatio(
            when (IntSetting.RENDERER_ASPECT_RATIO.int) {
                0 -> Rational(16, 9)
                1 -> Rational(4, 3)
                2 -> Rational(21, 9)
                3 -> Rational(16, 10)
                4 -> null // Stretch
                else -> Rational(16, 9)
            }
        )
        emulationActivity?.buildPictureInPictureParams()
        updateOrientation()
    }

    private fun updateFoldableLayout(
        emulationActivity: EmulationActivity,
        newLayoutInfo: WindowLayoutInfo
    ) {
        val isFolding =
            (newLayoutInfo.displayFeatures.find { it is FoldingFeature } as? FoldingFeature)?.let {
                if (it.isSeparating) {
                    emulationActivity.requestedOrientation =
                        ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
                    if (it.orientation == FoldingFeature.Orientation.HORIZONTAL) {
                        // Restrict emulation and overlays to the top of the screen
                        binding.emulationContainer.layoutParams.height = it.bounds.top
                        binding.overlayContainer.layoutParams.height = it.bounds.top
                        // Restrict input and menu drawer to the bottom of the screen
                        binding.inputContainer.layoutParams.height = it.bounds.bottom
                        binding.inGameMenu.layoutParams.height = it.bounds.bottom

                        isInFoldableLayout = true
                        binding.surfaceInputOverlay.layout = InputOverlay.FOLDABLE
                        refreshInputOverlay()
                    }
                }
                it.isSeparating
            } ?: false
        if (!isFolding) {
            binding.emulationContainer.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            binding.inputContainer.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            binding.overlayContainer.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            binding.inGameMenu.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
            isInFoldableLayout = false
            updateOrientation()
            onConfigurationChanged(resources.configuration)
        }
        binding.emulationContainer.requestLayout()
        binding.inputContainer.requestLayout()
        binding.overlayContainer.requestLayout()
        binding.inGameMenu.requestLayout()
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
                    val optionsArray = BooleanArray(Settings.overlayPreferences.size)
                    Settings.overlayPreferences.forEachIndexed { i, _ ->
                        optionsArray[i] = preferences.getBoolean("buttonToggle$i", i < 15)
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
                            Settings.overlayPreferences.forEachIndexed { i, _ ->
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

    @SuppressLint("SourceLockedOrientationActivity")
    private fun startConfiguringControls() {
        // Lock the current orientation to prevent editing inconsistencies
        if (IntSetting.RENDERER_SCREEN_LAYOUT.int == Settings.LayoutOption_Unspecified) {
            emulationActivity?.let {
                it.requestedOrientation =
                    if (resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT) {
                        ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
                    } else {
                        ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
                    }
            }
        }
        binding.doneControlConfig.visibility = View.VISIBLE
        binding.surfaceInputOverlay.setIsInEditMode(true)
    }

    private fun stopConfiguringControls() {
        binding.doneControlConfig.visibility = View.GONE
        binding.surfaceInputOverlay.setIsInEditMode(false)
        // Unlock the orientation if it was locked for editing
        if (IntSetting.RENDERER_SCREEN_LAYOUT.int == Settings.LayoutOption_Unspecified) {
            emulationActivity?.let {
                it.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
            }
        }
    }

    @SuppressLint("SetTextI18n")
    private fun adjustOverlay() {
        val adjustBinding = DialogOverlayAdjustBinding.inflate(layoutInflater)
        adjustBinding.apply {
            inputScaleSlider.apply {
                valueTo = 150F
                value = preferences.getInt(Settings.PREF_CONTROL_SCALE, 50).toFloat()
                addOnChangeListener(
                    Slider.OnChangeListener { _, value, _ ->
                        inputScaleValue.text = "${value.toInt()}%"
                        setControlScale(value.toInt())
                    }
                )
            }
            inputOpacitySlider.apply {
                valueTo = 100F
                value = preferences.getInt(Settings.PREF_CONTROL_OPACITY, 100).toFloat()
                addOnChangeListener(
                    Slider.OnChangeListener { _, value, _ ->
                        inputOpacityValue.text = "${value.toInt()}%"
                        setControlOpacity(value.toInt())
                    }
                )
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
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.inGameMenu
        ) { v: View, windowInsets: WindowInsetsCompat ->
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

                    State.PAUSED -> Log.warning(
                        "[EmulationFragment] Surface cleared while emulation paused."
                    )
                    else -> Log.warning(
                        "[EmulationFragment] Surface cleared while emulation stopped."
                    )
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
                    NativeLibrary.unpauseEmulation()
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
    }
}
