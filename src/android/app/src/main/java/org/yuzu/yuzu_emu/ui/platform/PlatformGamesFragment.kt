// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.platform

import android.database.Cursor
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import com.google.android.material.color.MaterialColors
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.adapters.GameAdapter
import org.yuzu.yuzu_emu.databinding.FragmentGridBinding
import org.yuzu.yuzu_emu.layout.AutofitGridLayoutManager

class PlatformGamesFragment : Fragment(), PlatformGamesView {
    private val presenter = PlatformGamesPresenter(this)

    private var _binding: FragmentGridBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        presenter.onCreateView()
        _binding = FragmentGridBinding.inflate(inflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        binding.gridGames.apply {
            layoutManager = AutofitGridLayoutManager(
                requireContext(),
                requireContext().resources.getDimensionPixelSize(R.dimen.card_width)
            )
            adapter = GameAdapter(requireActivity() as AppCompatActivity)
        }

        // Add swipe down to refresh gesture
        binding.swipeRefresh.setOnRefreshListener {
            refresh()
            binding.swipeRefresh.isRefreshing = false
        }

        // Set theme color to the refresh animation's background
        binding.swipeRefresh.setProgressBackgroundColorSchemeColor(
            MaterialColors.getColor(binding.swipeRefresh, R.attr.colorPrimary)
        )
        binding.swipeRefresh.setColorSchemeColors(
            MaterialColors.getColor(binding.swipeRefresh, R.attr.colorOnPrimary)
        )

        setInsets()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun refresh() {
        val databaseHelper = YuzuApplication.databaseHelper
        databaseHelper!!.scanLibrary(databaseHelper.writableDatabase)
        presenter.refresh()
        updateTextView()
    }

    override fun showGames(games: Cursor) {
        if (binding.gridGames.adapter != null) {
            (binding.gridGames.adapter as GameAdapter).swapCursor(games)
        }
        updateTextView()
    }

    private fun updateTextView() {
        if (_binding == null)
            return

        binding.gamelistEmptyText.visibility =
            if ((binding.gridGames.adapter as GameAdapter).itemCount == 0) View.VISIBLE else View.GONE
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.gridGames) { view: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.updatePadding(bottom = insets.bottom)
            windowInsets
        }
    }

    companion object {
        const val TAG = "PlatformGamesFragment"
    }
}
