// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.fetcher

import android.animation.LayoutTransition
import android.content.res.ColorStateList
import android.text.Html
import android.text.Html.FROM_HTML_MODE_COMPACT
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.Toast
import androidx.core.view.isVisible
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ItemReleaseBinding
import org.yuzu.yuzu_emu.fragments.DriverFetcherFragment.Release
import androidx.core.net.toUri
import androidx.transition.ChangeBounds
import androidx.transition.Fade
import androidx.transition.TransitionManager
import androidx.transition.TransitionSet
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.commonmark.parser.Parser
import org.commonmark.renderer.html.HtmlRenderer
import org.yuzu.yuzu_emu.databinding.DialogProgressBinding
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

class ReleaseAdapter(
    private val releases: List<Release>,
    private val activity: FragmentActivity,
    private val driverViewModel: DriverViewModel
) : RecyclerView.Adapter<ReleaseAdapter.ReleaseViewHolder>() {

    inner class ReleaseViewHolder(
        private val binding: ItemReleaseBinding
    ) : RecyclerView.ViewHolder(binding.root) {
        private var isPreview: Boolean = true
        private val client = OkHttpClient()
        private val markdownParser = Parser.builder().build()
        private val htmlRenderer = HtmlRenderer.builder().build()

        init {
            binding.root.let { root ->
                val layoutTransition = root.layoutTransition ?: LayoutTransition().apply {
                    enableTransitionType(LayoutTransition.CHANGING)
                    setDuration(125)
                }
                root.layoutTransition = layoutTransition
            }

            (binding.textBody.parent as ViewGroup).isTransitionGroup = false
            binding.containerDownloads.isTransitionGroup = false
        }

        fun bind(release: Release) {
            binding.textReleaseName.text = release.title
            binding.badgeLatest.isVisible = release.latest

            // truncates to 150 chars so it does not take up too much space.
            var bodyPreview = release.body.take(150)
            bodyPreview = bodyPreview.replace("#", "").removeSurrounding(" ")

            val body =
                bodyPreview.replace("\\r\\n", "\n").replace("\\n", "\n").replace("\n", "<br>")

            binding.textBody.text = Html.fromHtml(body, FROM_HTML_MODE_COMPACT)

            binding.textBody.setOnClickListener {
                TransitionManager.beginDelayedTransition(
                    binding.root,
                    TransitionSet().addTransition(Fade()).addTransition(ChangeBounds())
                        .setDuration(100)
                )

                isPreview = !isPreview
                if (isPreview) {
                    val body = bodyPreview.replace("\\r\\n", "\n").replace("\\n", "\n")
                        .replace("\n", "<br>")

                    binding.textBody.text = Html.fromHtml(body, FROM_HTML_MODE_COMPACT)
                    binding.textBody.maxLines = 3
                    binding.textBody.ellipsize = TextUtils.TruncateAt.END
                } else {
                    val body = release.body.replace("\\r\\n", "\n\n").replace("\\n", "\n\n")

                    try {
                        val doc = markdownParser.parse(body)
                        val html = htmlRenderer.render(doc)
                        binding.textBody.text = Html.fromHtml(html, Html.FROM_HTML_MODE_COMPACT)
                    } catch (e: Exception) {
                        e.printStackTrace()
                        binding.textBody.text = body
                    }

                    binding.textBody.maxLines = Integer.MAX_VALUE
                    binding.textBody.ellipsize = null
                }
            }

            val onDownloadsClick = {
                val isVisible = binding.containerDownloads.isVisible
                TransitionManager.beginDelayedTransition(
                    binding.root,
                    TransitionSet().addTransition(Fade()).addTransition(ChangeBounds())
                        .setDuration(100)
                )

                binding.containerDownloads.isVisible = !isVisible

                binding.imageDownloadsArrow.rotation = if (isVisible) 0f else 180f
                binding.buttonToggleDownloads.text =
                    if (isVisible) {
                        activity.getString(R.string.show_downloads)
                    } else {
                        activity.getString(R.string.hide_downloads)
                    }
            }

            binding.buttonToggleDownloads.setOnClickListener {
                onDownloadsClick()
            }

            binding.imageDownloadsArrow.setOnClickListener {
                onDownloadsClick()
            }

            binding.containerDownloads.removeAllViews()

            release.artifacts.forEach { artifact ->
                val button = MaterialButton(binding.root.context).apply {
                    text = artifact.name
                    setTextAppearance(
                        com.google.android.material.R.style.TextAppearance_Material3_LabelLarge
                    )
                    textAlignment = MaterialButton.TEXT_ALIGNMENT_VIEW_START
                    setBackgroundColor(
                        context.getColor(
                            com.google.android.material.R.color.m3_button_background_color_selector
                        )
                    )
                    setIconResource(R.drawable.ic_import)
                    iconTint = ColorStateList.valueOf(
                        MaterialColors.getColor(
                            this,
                            com.google.android.material.R.attr.colorPrimary
                        )
                    )

                    elevation = 6f
                    layoutParams = ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT
                    )
                    setOnClickListener {
                        val dialogBinding =
                            DialogProgressBinding.inflate(LayoutInflater.from(context))
                        dialogBinding.progressBar.isIndeterminate = true
                        dialogBinding.title.text = context.getString(R.string.installing_driver)
                        dialogBinding.status.text = context.getString(R.string.downloading)

                        val progressDialog = MaterialAlertDialogBuilder(context)
                            .setView(dialogBinding.root)
                            .setCancelable(false)
                            .create()

                        progressDialog.show()

                        CoroutineScope(Dispatchers.Main).launch {
                            try {
                                val request = Request.Builder()
                                    .url(artifact.url)
                                    .header("Accept", "application/octet-stream")
                                    .build()

                                val cacheDir = context.externalCacheDir ?: throw IOException(
                                    context.getString(R.string.failed_cache_dir)
                                )

                                cacheDir.mkdirs()

                                val file = File(cacheDir, artifact.name)

                                withContext(Dispatchers.IO) {
                                    client.newBuilder()
                                        .followRedirects(true)
                                        .followSslRedirects(true)
                                        .build()
                                        .newCall(request).execute().use { response ->
                                            if (!response.isSuccessful) {
                                                throw IOException("${response.code}")
                                            }

                                            response.body?.byteStream()?.use { input ->
                                                FileOutputStream(file).use { output ->
                                                    input.copyTo(output)
                                                }
                                            }
                                                ?: throw IOException(
                                                    context.getString(R.string.empty_response_body)
                                                )
                                        }
                                }

                                if (file.length() == 0L) {
                                    throw IOException(context.getString(R.string.driver_empty))
                                }

                                dialogBinding.status.text = context.getString(R.string.installing)

                                val driverData = GpuDriverHelper.getMetadataFromZip(file)
                                val driverPath =
                                    "${GpuDriverHelper.driverStoragePath}${FileUtil.getFilename(
                                        file.toUri()
                                    )}"

                                if (GpuDriverHelper.copyDriverToInternalStorage(file.toUri())) {
                                    driverViewModel.onDriverAdded(Pair(driverPath, driverData))

                                    progressDialog.dismiss()
                                    Toast.makeText(
                                        context,
                                        context.getString(
                                            R.string.successfully_installed,
                                            driverData.name
                                        ),
                                        Toast.LENGTH_SHORT
                                    ).show()
                                } else {
                                    throw IOException(
                                        context.getString(
                                            R.string.failed_install_driver,
                                            artifact.name
                                        )
                                    )
                                }
                            } catch (e: Exception) {
                                progressDialog.dismiss()

                                MaterialAlertDialogBuilder(context)
                                    .setTitle(context.getString(R.string.driver_failed_title))
                                    .setMessage(e.message)
                                    .setPositiveButton(R.string.ok) { dialog, _ ->
                                        dialog.cancel()
                                    }
                                    .show()
                            }
                        }
                    }
                }
                binding.containerDownloads.addView(button)
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ReleaseViewHolder {
        val binding = ItemReleaseBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return ReleaseViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ReleaseViewHolder, position: Int) {
        holder.bind(releases[position])
    }

    override fun getItemCount(): Int = releases.size
}
