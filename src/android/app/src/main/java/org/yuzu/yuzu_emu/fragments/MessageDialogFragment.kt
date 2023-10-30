// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.text.Html
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentActivity
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.model.MessageDialogViewModel

class MessageDialogFragment : DialogFragment() {
    private val messageDialogViewModel: MessageDialogViewModel by activityViewModels()

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val titleId = requireArguments().getInt(TITLE_ID)
        val titleString = requireArguments().getString(TITLE_STRING)!!
        val descriptionId = requireArguments().getInt(DESCRIPTION_ID)
        val descriptionString = requireArguments().getString(DESCRIPTION_STRING)!!
        val helpLinkId = requireArguments().getInt(HELP_LINK)

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setPositiveButton(R.string.close, null)

        if (titleId != 0) dialog.setTitle(titleId)
        if (titleString.isNotEmpty()) dialog.setTitle(titleString)

        if (descriptionId != 0) {
            dialog.setMessage(Html.fromHtml(getString(descriptionId), Html.FROM_HTML_MODE_LEGACY))
        }
        if (descriptionString.isNotEmpty()) dialog.setMessage(descriptionString)

        if (helpLinkId != 0) {
            dialog.setNeutralButton(R.string.learn_more) { _, _ ->
                openLink(getString(helpLinkId))
            }
        }

        return dialog.show()
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        messageDialogViewModel.dismissAction.invoke()
        messageDialogViewModel.clear()
    }

    private fun openLink(link: String) {
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(link))
        startActivity(intent)
    }

    companion object {
        const val TAG = "MessageDialogFragment"

        private const val TITLE_ID = "Title"
        private const val TITLE_STRING = "TitleString"
        private const val DESCRIPTION_ID = "DescriptionId"
        private const val DESCRIPTION_STRING = "DescriptionString"
        private const val HELP_LINK = "Link"

        fun newInstance(
            activity: FragmentActivity,
            titleId: Int = 0,
            titleString: String = "",
            descriptionId: Int = 0,
            descriptionString: String = "",
            helpLinkId: Int = 0,
            dismissAction: () -> Unit = {}
        ): MessageDialogFragment {
            val dialog = MessageDialogFragment()
            val bundle = Bundle()
            bundle.apply {
                putInt(TITLE_ID, titleId)
                putString(TITLE_STRING, titleString)
                putInt(DESCRIPTION_ID, descriptionId)
                putString(DESCRIPTION_STRING, descriptionString)
                putInt(HELP_LINK, helpLinkId)
            }
            ViewModelProvider(activity)[MessageDialogViewModel::class.java].dismissAction =
                dismissAction
            dialog.arguments = bundle
            return dialog
        }
    }
}
