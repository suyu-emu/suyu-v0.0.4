// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.platform

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.color.MaterialColors
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.GameAdapter
import org.yuzu.yuzu_emu.databinding.FragmentGridBinding
import org.yuzu.yuzu_emu.layout.AutofitGridLayoutManager
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.utils.GameHelper

class PlatformGamesFragment : Fragment() {
    private var _binding: FragmentGridBinding? = null
    private val binding get() = _binding!!

    private lateinit var gamesViewModel: GamesViewModel

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentGridBinding.inflate(inflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        gamesViewModel = ViewModelProvider(requireActivity())[GamesViewModel::class.java]

        binding.gridGames.apply {
            layoutManager = AutofitGridLayoutManager(
                requireContext(),
                requireContext().resources.getDimensionPixelSize(R.dimen.card_width)
            )
            adapter =
                GameAdapter(requireActivity() as AppCompatActivity, gamesViewModel.games.value!!)
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

        gamesViewModel.games.observe(viewLifecycleOwner) {
            (binding.gridGames.adapter as GameAdapter).swapData(it)
            updateTextView()
        }

        setInsets()

        refresh()
    }

    override fun onResume() {
        super.onResume()
        refresh()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    fun refresh() {
        gamesViewModel.setGames(GameHelper.getGames())
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
