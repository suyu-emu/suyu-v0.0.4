// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.snackbar.Snackbar
import com.google.android.material.transition.MaterialSharedAxis
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.FreedrenoPresetAdapter
import org.yuzu.yuzu_emu.adapters.FreedrenoVariableAdapter
import org.yuzu.yuzu_emu.databinding.FragmentFreedrenoSettingsBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.NativeFreedrenoConfig
import org.yuzu.yuzu_emu.utils.FreedrenoPresets


class FreedrenoSettingsFragment : Fragment() {
    private var _binding: FragmentFreedrenoSettingsBinding? = null
    private val binding get() = _binding!!
    private val args by navArgs<FreedrenoSettingsFragmentArgs>()
    private val game: Game? get() = args.game
    private val isPerGameConfig: Boolean get() = game != null

    private lateinit var presetAdapter: FreedrenoPresetAdapter
    private lateinit var settingsAdapter: FreedrenoVariableAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentFreedrenoSettingsBinding.inflate(layoutInflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        NativeFreedrenoConfig.setFreedrenoBasePath(requireContext().cacheDir.absolutePath)
        NativeFreedrenoConfig.initializeFreedrenoConfig()

        if (isPerGameConfig) {
            NativeFreedrenoConfig.loadPerGameConfig(game!!.programIdHex)
        } else {
            NativeFreedrenoConfig.reloadFreedrenoConfig()
        }

        setupToolbar()
        setupAdapters()
        loadCurrentSettings()
        setupButtonListeners()
        setupWindowInsets()
    }

    private fun setupToolbar() {
        binding.toolbarFreedreno.setNavigationOnClickListener {
            requireActivity().onBackPressedDispatcher.onBackPressed()
        }
        if (isPerGameConfig) {
            binding.toolbarFreedreno.title = getString(R.string.freedreno_per_game_title)
            binding.toolbarFreedreno.subtitle = game!!.title
        }
    }

    private fun setupAdapters() {
        // Setup presets adapter (horizontal list)
        presetAdapter = FreedrenoPresetAdapter { preset ->
            applyPreset(preset)
        }
        binding.listFreedrenoPresets.apply {
            adapter = presetAdapter
            layoutManager = LinearLayoutManager(requireContext(), LinearLayoutManager.HORIZONTAL, false)
        }
        presetAdapter.submitList(FreedrenoPresets.ALL_PRESETS)

        // Setup current settings adapter (vertical list)
        settingsAdapter = FreedrenoVariableAdapter(requireContext()) { variable, onDelete ->
            onDelete()
            loadCurrentSettings() // Refresh list after deletion
        }
        binding.listFreedrenoSettings.apply {
            adapter = settingsAdapter
            layoutManager = LinearLayoutManager(requireContext())
        }
    }

    private fun loadCurrentSettings() {
        // Load all currently set environment variables
        val variables = mutableListOf<FreedrenoVariable>()

        // Common variables to check
        val commonVars = listOf(
            "TU_DEBUG", "FD_MESA_DEBUG", "IR3_SHADER_DEBUG",
            "FD_RD_DUMP", "FD_RD_DUMP_FRAMES", "FD_RD_DUMP_TESTNAME",
            "TU_BREADCRUMBS"
        )

        for (varName in commonVars) {
            if (NativeFreedrenoConfig.isFreedrenoEnvSet(varName)) {
                val value = NativeFreedrenoConfig.getFreedrenoEnv(varName)
                variables.add(FreedrenoVariable(varName, value))
            }
        }

        settingsAdapter.submitList(variables)
    }

    private fun setupButtonListeners() {
        binding.buttonAddVariable.setOnClickListener {
            val varName = binding.variableNameInput.text.toString().trim()
            val varValue = binding.variableValueInput.text.toString().trim()

            if (varName.isEmpty()) {
                showSnackbar(getString(R.string.freedreno_error_empty_name))
                return@setOnClickListener
            }

            if (NativeFreedrenoConfig.setFreedrenoEnv(varName, varValue)) {
                showSnackbar(getString(R.string.freedreno_variable_added, varName))
                binding.variableNameInput.text?.clear()
                binding.variableValueInput.text?.clear()
                loadCurrentSettings()
            } else {
                showSnackbar(getString(R.string.freedreno_error_setting_variable))
            }
        }

        binding.buttonClearAll.setOnClickListener {
            NativeFreedrenoConfig.clearAllFreedrenoEnv()
            showSnackbar(getString(R.string.freedreno_cleared_all))
            loadCurrentSettings()
        }

        binding.buttonSave.setOnClickListener {
            if (isPerGameConfig) {
                NativeFreedrenoConfig.savePerGameConfig(game!!.programIdHex)
                showSnackbar(getString(R.string.freedreno_per_game_saved))
            } else {
                NativeFreedrenoConfig.saveFreedrenoConfig()
                showSnackbar(getString(R.string.freedreno_saved))
            }
        }
    }

    private fun applyPreset(preset: org.yuzu.yuzu_emu.utils.FreedrenoPreset) {
        // Clear all first for consistency
        NativeFreedrenoConfig.clearAllFreedrenoEnv()

        // Apply all variables in the preset
        for ((varName, varValue) in preset.variables) {
            NativeFreedrenoConfig.setFreedrenoEnv(varName, varValue)
        }

        showSnackbar(getString(R.string.freedreno_preset_applied, preset.name))
        loadCurrentSettings()
    }

    private fun setupWindowInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { _, insets ->
            val systemInsets = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            binding.root.updatePadding(
                left = systemInsets.left,
                right = systemInsets.right,
                bottom = systemInsets.bottom
            )
            insets
        }
    }

    private fun showSnackbar(message: String) {
        Snackbar.make(binding.root, message, Snackbar.LENGTH_SHORT).show()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}

/**
 * Data class representing a Freedreno environment variable.
 */
data class FreedrenoVariable(
    val name: String,
    val value: String
)
