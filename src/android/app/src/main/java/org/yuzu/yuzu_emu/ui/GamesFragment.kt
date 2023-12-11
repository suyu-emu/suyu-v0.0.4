// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui

import android.annotation.SuppressLint
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.MarginLayoutParams
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.google.android.material.color.MaterialColors
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.GameAdapter
import org.yuzu.yuzu_emu.databinding.FragmentGamesBinding
import org.yuzu.yuzu_emu.layout.AutofitGridLayoutManager
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel

class GamesFragment : Fragment() {
    private var _binding: FragmentGamesBinding? = null
    private val binding get() = _binding!!

    private val gamesViewModel: GamesViewModel by activityViewModels()
    private val homeViewModel: HomeViewModel by activityViewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentGamesBinding.inflate(inflater)
        return binding.root
    }

    // This is using the correct scope, lint is just acting up
    @SuppressLint("UnsafeRepeatOnLifecycleDetector")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        homeViewModel.setNavigationVisibility(visible = true, animated = true)
        homeViewModel.setStatusBarShadeVisibility(true)

        binding.gridGames.apply {
            layoutManager = AutofitGridLayoutManager(
                requireContext(),
                requireContext().resources.getDimensionPixelSize(R.dimen.card_width)
            )
            adapter = GameAdapter(requireActivity() as AppCompatActivity)
        }

        binding.swipeRefresh.apply {
            // Add swipe down to refresh gesture
            setOnRefreshListener {
                gamesViewModel.reloadGames(false)
            }

            // Set theme color to the refresh animation's background
            setProgressBackgroundColorSchemeColor(
                MaterialColors.getColor(
                    binding.swipeRefresh,
                    com.google.android.material.R.attr.colorPrimary
                )
            )
            setColorSchemeColors(
                MaterialColors.getColor(
                    binding.swipeRefresh,
                    com.google.android.material.R.attr.colorOnPrimary
                )
            )

            // Make sure the loading indicator appears even if the layout is told to refresh before being fully drawn
            post {
                if (_binding == null) {
                    return@post
                }
                binding.swipeRefresh.isRefreshing = gamesViewModel.isReloading.value
            }
        }

        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.RESUMED) {
                    gamesViewModel.isReloading.collect { binding.swipeRefresh.isRefreshing = it }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.RESUMED) {
                    gamesViewModel.games.collectLatest {
                        (binding.gridGames.adapter as GameAdapter).submitList(it)
                        if (it.isEmpty()) {
                            binding.noticeText.visibility = View.VISIBLE
                        } else {
                            binding.noticeText.visibility = View.GONE
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.RESUMED) {
                    gamesViewModel.shouldSwapData.collect {
                        if (it) {
                            (binding.gridGames.adapter as GameAdapter).submitList(
                                gamesViewModel.games.value
                            )
                            gamesViewModel.setShouldSwapData(false)
                        }
                    }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.RESUMED) {
                    gamesViewModel.shouldScrollToTop.collect {
                        if (it) {
                            scrollToTop()
                            gamesViewModel.setShouldScrollToTop(false)
                        }
                    }
                }
            }
        }

        setInsets()
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
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { view: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            val extraListSpacing = resources.getDimensionPixelSize(R.dimen.spacing_large)
            val spacingNavigation = resources.getDimensionPixelSize(R.dimen.spacing_navigation)
            val spacingNavigationRail =
                resources.getDimensionPixelSize(R.dimen.spacing_navigation_rail)

            binding.gridGames.updatePadding(
                top = barInsets.top + extraListSpacing,
                bottom = barInsets.bottom + spacingNavigation + extraListSpacing
            )

            binding.swipeRefresh.setProgressViewEndTarget(
                false,
                barInsets.top + resources.getDimensionPixelSize(R.dimen.spacing_refresh_end)
            )

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right
            val mlpSwipe = binding.swipeRefresh.layoutParams as MarginLayoutParams
            if (ViewCompat.getLayoutDirection(view) == ViewCompat.LAYOUT_DIRECTION_LTR) {
                mlpSwipe.leftMargin = leftInsets + spacingNavigationRail
                mlpSwipe.rightMargin = rightInsets
            } else {
                mlpSwipe.leftMargin = leftInsets
                mlpSwipe.rightMargin = rightInsets + spacingNavigationRail
            }
            binding.swipeRefresh.layoutParams = mlpSwipe

            binding.noticeText.updatePadding(bottom = spacingNavigation)

            windowInsets
        }
}
