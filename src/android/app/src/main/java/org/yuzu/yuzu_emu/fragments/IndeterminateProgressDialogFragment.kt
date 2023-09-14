// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.os.Bundle
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.databinding.DialogProgressBarBinding
import org.yuzu.yuzu_emu.model.TaskViewModel

class IndeterminateProgressDialogFragment : DialogFragment() {
    private val taskViewModel: TaskViewModel by activityViewModels()

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val titleId = requireArguments().getInt(TITLE)

        val progressBinding = DialogProgressBarBinding.inflate(layoutInflater)
        progressBinding.progressBar.isIndeterminate = true
        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(titleId)
            .setView(progressBinding.root)
            .create()
        dialog.setCanceledOnTouchOutside(false)

        viewLifecycleOwner.lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.CREATED) {
                taskViewModel.isComplete.collect {
                    if (it) {
                        dialog.dismiss()
                        when (val result = taskViewModel.result.value) {
                            is String -> Toast.makeText(requireContext(), result, Toast.LENGTH_LONG)
                                .show()

                            is MessageDialogFragment -> result.show(
                                requireActivity().supportFragmentManager,
                                MessageDialogFragment.TAG
                            )
                        }
                        taskViewModel.clear()
                    }
                }
            }
        }

        if (!taskViewModel.isRunning.value) {
            taskViewModel.runTask()
        }
        return dialog
    }

    companion object {
        const val TAG = "IndeterminateProgressDialogFragment"

        private const val TITLE = "Title"

        fun newInstance(
            activity: AppCompatActivity,
            titleId: Int,
            task: () -> Any
        ): IndeterminateProgressDialogFragment {
            val dialog = IndeterminateProgressDialogFragment()
            val args = Bundle()
            ViewModelProvider(activity)[TaskViewModel::class.java].task = task
            args.putInt(TITLE, titleId)
            dialog.arguments = args
            return dialog
        }
    }
}
