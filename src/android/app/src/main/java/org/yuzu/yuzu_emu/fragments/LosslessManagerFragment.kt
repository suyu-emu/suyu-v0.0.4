// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.HomeSettingAdapter
import org.yuzu.yuzu_emu.databinding.FragmentLosslessManagerBinding
import org.yuzu.yuzu_emu.features.fetcher.SpacingItemDecoration
import org.yuzu.yuzu_emu.model.HomeSetting
import org.yuzu.yuzu_emu.utils.LosslessScalingHelper
import org.yuzu.yuzu_emu.utils.ViewUtils.updateMargins
import org.yuzu.yuzu_emu.utils.collect

class LosslessManagerFragment : Fragment() {
    private var _binding: FragmentLosslessManagerBinding? = null
    private val binding get() = _binding!!

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
        _binding = FragmentLosslessManagerBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.toolbarLossless.setNavigationOnClickListener {
            requireActivity().onBackPressedDispatcher.onBackPressed()
        }

        binding.losslessOptionsList.apply {
            layoutManager =
                GridLayoutManager(requireContext(), resources.getInteger(R.integer.grid_columns))
            addItemDecoration(
                SpacingItemDecoration(resources.getDimensionPixelSize(R.dimen.spacing_small))
            )
        }

        LosslessScalingHelper.statusText.collect(viewLifecycleOwner) { refreshOptions() }

        setInsets()
    }

    private fun refreshOptions() {
        binding.losslessOptionsList.adapter = HomeSettingAdapter(
            requireActivity() as AppCompatActivity,
            viewLifecycleOwner,
            buildOptions()
        )
    }

    private fun buildOptions(): List<HomeSetting> {
        val installed = LosslessScalingHelper.isInstalled()
        return listOf(
            HomeSetting(
                if (installed) R.string.lossless_scaling_replace else R.string.lossless_scaling_install,
                if (installed) {
                    R.string.lossless_scaling_replace_description
                } else {
                    R.string.lossless_scaling_install_description
                },
                R.drawable.ic_install,
                { dllPickerLauncher.launch(arrayOf("*/*")) },
                { !NativeLibrary.isRunning() },
                R.string.lossless_scaling_locked,
                R.string.lossless_scaling_locked_description,
                LosslessScalingHelper.statusText
            ),
            HomeSetting(
                R.string.lossless_scaling_remove,
                R.string.lossless_scaling_remove_description,
                R.drawable.ic_delete,
                { confirmRemoval() },
                { installed && !NativeLibrary.isRunning() },
                if (installed) {
                    R.string.lossless_scaling_locked
                } else {
                    R.string.lossless_scaling_remove_unavailable
                },
                if (installed) {
                    R.string.lossless_scaling_locked_description
                } else {
                    R.string.lossless_scaling_remove_unavailable_description
                }
            )
        )
    }

    private fun confirmRemoval() {
        MessageDialogFragment.newInstance(
            requireActivity(),
            titleId = R.string.lossless_scaling_remove,
            descriptionId = R.string.lossless_scaling_remove_confirmation,
            positiveButtonTitleId = R.string.lossless_scaling_remove,
            positiveAction = { LosslessScalingHelper.remove() },
            showNegativeButton = true,
            negativeAction = {}
        ).show(parentFragmentManager, MessageDialogFragment.TAG)
    }

    private val dllPickerLauncher =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null) {
                return@registerForActivityResult
            }

            val resultStrings = resources.getStringArray(R.array.losslessDllResults)
            ProgressDialogFragment.newInstance(
                requireActivity(),
                R.string.lossless_scaling_installing,
                false
            ) { _, _ ->
                val installResult = LosslessScalingHelper.install(result)
                if (installResult == LosslessScalingHelper.RESULT_OK) {
                    getString(R.string.lossless_scaling_install_success)
                } else {
                    MessageDialogFragment.newInstance(
                        titleId = R.string.lossless_scaling_install_failed,
                        descriptionString = resultStrings[installResult]
                    )
                }
            }.show(parentFragmentManager, ProgressDialogFragment.TAG)
        }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { _, windowInsets ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            binding.appbarLossless.updateMargins(
                left = barInsets.left + cutoutInsets.left,
                right = barInsets.right + cutoutInsets.right
            )

            binding.scrollViewLossless.updatePadding(bottom = barInsets.bottom)

            binding.losslessOptionsList.updatePadding(
                left = barInsets.left + cutoutInsets.left,
                right = barInsets.right + cutoutInsets.right
            )

            windowInsets
        }
}
