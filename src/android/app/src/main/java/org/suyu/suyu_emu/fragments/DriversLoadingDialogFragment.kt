// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.fragments

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.suyu.suyu_emu.R
import org.suyu.suyu_emu.databinding.DialogProgressBarBinding
import org.suyu.suyu_emu.model.DriverViewModel
import org.suyu.suyu_emu.utils.collect

class DriversLoadingDialogFragment : DialogFragment() {
    private val driverViewModel: DriverViewModel by activityViewModels()

    private lateinit var binding: DialogProgressBarBinding

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        binding = DialogProgressBarBinding.inflate(layoutInflater)
        binding.progressBar.isIndeterminate = true

        isCancelable = false

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.loading)
            .setView(binding.root)
            .create()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View = binding.root

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        driverViewModel.isInteractionAllowed.collect(viewLifecycleOwner) { if (it) dismiss() }
    }

    companion object {
        const val TAG = "DriversLoadingDialogFragment"
    }
}
