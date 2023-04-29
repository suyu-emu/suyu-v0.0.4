// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.MarginLayoutParams
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import com.google.android.material.color.MaterialColors
import com.google.android.material.search.SearchView
import com.google.android.material.search.SearchView.TransitionState
import com.google.android.material.transition.MaterialFadeThrough
import info.debatty.java.stringsimilarity.Jaccard
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.GameAdapter
import org.yuzu.yuzu_emu.databinding.FragmentGamesBinding
import org.yuzu.yuzu_emu.layout.AutofitGridLayoutManager
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import java.util.Locale

class GamesFragment : Fragment() {
    private var _binding: FragmentGamesBinding? = null
    private val binding get() = _binding!!

    private val gamesViewModel: GamesViewModel by activityViewModels()
    private val homeViewModel: HomeViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialFadeThrough()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentGamesBinding.inflate(inflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        homeViewModel.setNavigationVisibility(visible = true, animated = false)

        binding.gridGames.apply {
            layoutManager = AutofitGridLayoutManager(
                requireContext(),
                requireContext().resources.getDimensionPixelSize(R.dimen.card_width)
            )
            adapter = GameAdapter(requireActivity() as AppCompatActivity)
        }

        // Add swipe down to refresh gesture
        binding.swipeRefresh.setOnRefreshListener {
            gamesViewModel.reloadGames(false)
        }

        // Set theme color to the refresh animation's background
        binding.swipeRefresh.setProgressBackgroundColorSchemeColor(
            MaterialColors.getColor(binding.swipeRefresh, R.attr.colorPrimary)
        )
        binding.swipeRefresh.setColorSchemeColors(
            MaterialColors.getColor(binding.swipeRefresh, R.attr.colorOnPrimary)
        )

        // Watch for when we get updates to any of our games lists
        gamesViewModel.isReloading.observe(viewLifecycleOwner) { isReloading ->
            binding.swipeRefresh.isRefreshing = isReloading
        }
        gamesViewModel.games.observe(viewLifecycleOwner) {
            (binding.gridGames.adapter as GameAdapter).submitList(it)
            if (it.isEmpty()) {
                binding.noticeText.visibility = View.VISIBLE
            } else {
                binding.noticeText.visibility = View.GONE
            }
        }

        gamesViewModel.shouldSwapData.observe(viewLifecycleOwner) { shouldSwapData ->
            if (shouldSwapData) {
                (binding.gridGames.adapter as GameAdapter).submitList(gamesViewModel.games.value)
                gamesViewModel.setShouldSwapData(false)
            }
        }

        // Check if the user reselected the games menu item and then scroll to top of the list
        gamesViewModel.shouldScrollToTop.observe(viewLifecycleOwner) { shouldScroll ->
            if (shouldScroll) {
                scrollToTop()
                gamesViewModel.setShouldScrollToTop(false)
            }
        }

        setInsets()

        // Make sure the loading indicator appears even if the layout is told to refresh before being fully drawn
        binding.swipeRefresh.post {
            if (_binding == null) {
                return@post
            }
            binding.swipeRefresh.isRefreshing = gamesViewModel.isReloading.value!!
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun scrollToTop() {
        if (_binding != null) {
            binding.gridGames.smoothScrollToPosition(0)
        }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.root) { view: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val extraListSpacing = resources.getDimensionPixelSize(R.dimen.spacing_large)

            binding.gridGames.updatePadding(
                top = insets.top + extraListSpacing,
                bottom = insets.bottom + resources.getDimensionPixelSize(R.dimen.spacing_navigation) + extraListSpacing
            )

            binding.swipeRefresh.setProgressViewEndTarget(
                false,
                insets.top + resources.getDimensionPixelSize(R.dimen.spacing_refresh_end)
            )

            val mlpSwipe = binding.swipeRefresh.layoutParams as MarginLayoutParams
            mlpSwipe.rightMargin = insets.right
            mlpSwipe.leftMargin = insets.left
            binding.swipeRefresh.layoutParams = mlpSwipe

            windowInsets
        }
}
