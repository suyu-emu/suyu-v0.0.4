// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.disk_shader_cache.ui

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.databinding.DialogProgressBarBinding
import org.yuzu.yuzu_emu.disk_shader_cache.DiskShaderCacheProgress
import org.yuzu.yuzu_emu.disk_shader_cache.ShaderProgressViewModel

class ShaderProgressDialogFragment : DialogFragment() {
    private var _binding: DialogProgressBarBinding? = null
    private val binding get() = _binding!!

    private lateinit var alertDialog: AlertDialog

    private lateinit var shaderProgressViewModel: ShaderProgressViewModel

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogProgressBarBinding.inflate(layoutInflater)
        shaderProgressViewModel =
            ViewModelProvider(requireActivity())[ShaderProgressViewModel::class.java]

        val title = requireArguments().getString(TITLE)
        val message = requireArguments().getString(MESSAGE)

        isCancelable = false
        alertDialog = MaterialAlertDialogBuilder(requireActivity())
            .setView(binding.root)
            .setTitle(title)
            .setMessage(message)
            .create()
        return alertDialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        shaderProgressViewModel.progress.observe(viewLifecycleOwner) { progress ->
            binding.progressBar.progress = progress
            setUpdateText()
        }
        shaderProgressViewModel.max.observe(viewLifecycleOwner) { max ->
            binding.progressBar.max = max
            setUpdateText()
        }
        shaderProgressViewModel.message.observe(viewLifecycleOwner) { msg ->
            alertDialog.setMessage(msg)
        }
        synchronized(DiskShaderCacheProgress.finishLock) {
            DiskShaderCacheProgress.finishLock.notifyAll()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    fun onUpdateProgress(msg: String, progress: Int, max: Int) {
        shaderProgressViewModel.setProgress(progress)
        shaderProgressViewModel.setMax(max)
        shaderProgressViewModel.setMessage(msg)
    }

    private fun setUpdateText() {
        binding.progressText.text = String.format(
            "%d/%d",
            shaderProgressViewModel.progress.value,
            shaderProgressViewModel.max.value
        )
    }

    companion object {
        const val TAG = "ProgressDialogFragment"
        const val TITLE = "title"
        const val MESSAGE = "message"

        fun newInstance(title: String, message: String): ShaderProgressDialogFragment {
            val frag = ShaderProgressDialogFragment()
            val args = Bundle()
            args.putString(TITLE, title)
            args.putString(MESSAGE, message)
            frag.arguments = args
            return frag
        }
    }
}
