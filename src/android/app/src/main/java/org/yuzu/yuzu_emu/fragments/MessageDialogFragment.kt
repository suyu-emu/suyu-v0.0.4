// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R

class MessageDialogFragment : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val titleId = requireArguments().getInt(TITLE)
        val descriptionId = requireArguments().getInt(DESCRIPTION)
        val helpLinkId = requireArguments().getInt(HELP_LINK)

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setPositiveButton(R.string.close, null)
            .setTitle(titleId)
            .setMessage(descriptionId)

        if (helpLinkId != 0) {
            dialog.setNeutralButton(R.string.learn_more) { _, _ ->
                openLink(getString(helpLinkId))
            }
        }

        return dialog.show()
    }

    private fun openLink(link: String) {
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(link))
        startActivity(intent)
    }

    companion object {
        const val TAG = "MessageDialogFragment"

        private const val TITLE = "Title"
        private const val DESCRIPTION = "Description"
        private const val HELP_LINK = "Link"

        fun newInstance(
            titleId: Int,
            descriptionId: Int,
            helpLinkId: Int = 0
        ): MessageDialogFragment {
            val dialog = MessageDialogFragment()
            val bundle = Bundle()
            bundle.apply {
                putInt(TITLE, titleId)
                putInt(DESCRIPTION, descriptionId)
                putInt(HELP_LINK, helpLinkId)
            }
            dialog.arguments = bundle
            return dialog
        }
    }
}
