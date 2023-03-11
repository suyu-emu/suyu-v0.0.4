// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.content.Context
import android.content.IntentFilter
import android.content.SharedPreferences
import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.view.*
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.fragment.app.Fragment
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.overlay.InputOverlay
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.DirectoryInitialization.DirectoryInitializationState
import org.yuzu.yuzu_emu.utils.DirectoryStateReceiver
import org.yuzu.yuzu_emu.utils.Log

class EmulationFragment : Fragment(), SurfaceHolder.Callback, Choreographer.FrameCallback {
    private lateinit var preferences: SharedPreferences
    private var inputOverlay: InputOverlay? = null
    private lateinit var emulationState: EmulationState
    private var directoryStateReceiver: DirectoryStateReceiver? = null
    private var emulationActivity: EmulationActivity? = null
    private lateinit var perfStats: TextView
    private var perfStatsUpdater: (() -> Unit)? = null

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
        val gamePath = requireArguments().getString(KEY_GAMEPATH)
        emulationState = EmulationState(gamePath)
    }

    /**
     * Initialize the UI and start emulation in here.
     */
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val contents = inflater.inflate(R.layout.fragment_emulation, container, false)
        val surfaceView = contents.findViewById<SurfaceView>(R.id.surface_emulation)
        surfaceView.holder.addCallback(this)
        inputOverlay = contents.findViewById(R.id.surface_input_overlay)
        perfStats = contents.findViewById(R.id.show_fps_text)
        perfStats.setTextColor(Color.YELLOW)
        val doneButton = contents.findViewById<Button>(R.id.done_control_config)
        doneButton?.setOnClickListener { stopConfiguringControls() }

        // Setup overlay.
        resetInputOverlay()
        updateShowFpsOverlay()

        // The new Surface created here will get passed to the native code via onSurfaceChanged.
        return contents
    }

    override fun onResume() {
        super.onResume()
        Choreographer.getInstance().postFrameCallback(this)
        if (DirectoryInitialization.areDirectoriesReady()) {
            emulationState.run(emulationActivity!!.isActivityRecreated)
        } else {
            setupDirectoriesThenStartEmulation()
        }
    }

    override fun onPause() {
        if (directoryStateReceiver != null) {
            LocalBroadcastManager.getInstance(requireActivity()).unregisterReceiver(
                directoryStateReceiver!!
            )
            directoryStateReceiver = null
        }
        if (emulationState.isRunning) {
            emulationState.pause()
        }
        Choreographer.getInstance().removeFrameCallback(this)
        super.onPause()
    }

    override fun onDetach() {
        NativeLibrary.clearEmulationActivity()
        super.onDetach()
    }

    private fun setupDirectoriesThenStartEmulation() {
        val statusIntentFilter = IntentFilter(
            DirectoryInitialization.BROADCAST_ACTION
        )
        directoryStateReceiver =
            DirectoryStateReceiver { directoryInitializationState: DirectoryInitializationState ->
                if (directoryInitializationState ==
                    DirectoryInitializationState.YUZU_DIRECTORIES_INITIALIZED
                ) {
                    emulationState.run(emulationActivity!!.isActivityRecreated)
                } else if (directoryInitializationState ==
                    DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE
                ) {
                    Toast.makeText(
                        context,
                        R.string.external_storage_not_mounted,
                        Toast.LENGTH_SHORT
                    )
                        .show()
                }
            }

        // Registers the DirectoryStateReceiver and its intent filters
        LocalBroadcastManager.getInstance(requireActivity()).registerReceiver(
            directoryStateReceiver!!,
            statusIntentFilter
        )
        DirectoryInitialization.start(requireContext())
    }

    fun refreshInputOverlay() {
        inputOverlay!!.refreshControls()
    }

    fun resetInputOverlay() {
        // Reset button scale
        preferences.edit()
            .putInt(Settings.PREF_CONTROL_SCALE, 50)
            .apply()
        inputOverlay!!.resetButtonPlacement()
    }

    private fun updateShowFpsOverlay() {
        // TODO: Create a setting so that this actually works...
        if (true) {
            val SYSTEM_FPS = 0
            val FPS = 1
            val FRAMETIME = 2
            val SPEED = 3
            perfStatsUpdater = {
                val perfStats = NativeLibrary.GetPerfStats()
                if (perfStats[FPS] > 0) {
                    this.perfStats.text = String.format("FPS: %.1f", perfStats[FPS])
                }
                perfStatsUpdateHandler.postDelayed(perfStatsUpdater!!, 100)
            }
            perfStatsUpdateHandler.post(perfStatsUpdater!!)
            perfStats.visibility = View.VISIBLE
        } else {
            if (perfStatsUpdater != null) {
                perfStatsUpdateHandler.removeCallbacks(perfStatsUpdater!!)
            }
            perfStats.visibility = View.GONE
        }
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

    override fun doFrame(frameTimeNanos: Long) {
        Choreographer.getInstance().postFrameCallback(this)
        NativeLibrary.DoFrame()
    }

    fun stopEmulation() {
        emulationState.stop()
    }

    fun startConfiguringControls() {
        requireView().findViewById<View>(R.id.done_control_config).visibility =
            View.VISIBLE
        inputOverlay!!.setIsInEditMode(true)
    }

    fun stopConfiguringControls() {
        requireView().findViewById<View>(R.id.done_control_config).visibility = View.GONE
        inputOverlay!!.setIsInEditMode(false)
    }

    val isConfiguringControls: Boolean
        get() = inputOverlay!!.isInEditMode

    private class EmulationState(private val mGamePath: String?) {
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
                state = State.STOPPED
                NativeLibrary.StopEmulation()
            } else {
                Log.warning("[EmulationFragment] Stop called while already stopped.")
            }
        }

        // State changing methods
        @Synchronized
        fun pause() {
            if (state != State.PAUSED) {
                state = State.PAUSED
                Log.debug("[EmulationFragment] Pausing emulation.")

                // Release the surface before pausing, since emulation has to be running for that.
                NativeLibrary.SurfaceDestroyed()
                NativeLibrary.PauseEmulation()
            } else {
                Log.warning("[EmulationFragment] Pause called while already paused.")
            }
        }

        @Synchronized
        fun run(isActivityRecreated: Boolean) {
            if (isActivityRecreated) {
                if (NativeLibrary.IsRunning()) {
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
                        NativeLibrary.SurfaceDestroyed()
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
                    NativeLibrary.SurfaceChanged(surface)
                    val mEmulationThread = Thread({
                        Log.debug("[EmulationFragment] Starting emulation thread.")
                        NativeLibrary.Run(mGamePath)
                    }, "NativeEmulation")
                    mEmulationThread.start()
                }
                State.PAUSED -> {
                    Log.debug("[EmulationFragment] Resuming emulation.")
                    NativeLibrary.SurfaceChanged(surface)
                    NativeLibrary.UnPauseEmulation()
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
        private const val KEY_GAMEPATH = "gamepath"
        private val perfStatsUpdateHandler = Handler()

        fun newInstance(gamePath: String?): EmulationFragment {
            val args = Bundle()
            args.putString(KEY_GAMEPATH, gamePath)
            val fragment = EmulationFragment()
            fragment.arguments = args
            return fragment
        }
    }
}
