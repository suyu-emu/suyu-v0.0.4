package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.databinding.DialogProgressBarBinding

class IndeterminateProgressDialogFragment : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val titleId = requireArguments().getInt(TITLE)

        val progressBinding = DialogProgressBarBinding.inflate(layoutInflater)
        progressBinding.progressBar.isIndeterminate = true
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(titleId)
            .setView(progressBinding.root)
            .show()
    }

    companion object {
        const val TAG = "IndeterminateProgressDialogFragment"

        private const val TITLE = "Title"

        fun newInstance(
            titleId: Int,
        ): IndeterminateProgressDialogFragment {
            val dialog = IndeterminateProgressDialogFragment()
            val args = Bundle()
            args.putInt(TITLE, titleId)
            dialog.arguments = args
            return dialog
        }
    }
}