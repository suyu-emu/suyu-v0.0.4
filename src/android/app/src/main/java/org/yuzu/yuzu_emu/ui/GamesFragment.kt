// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import com.google.android.material.color.MaterialColors
import com.google.android.material.search.SearchView
import com.google.android.material.search.SearchView.TransitionState
import info.debatty.java.stringsimilarity.Jaccard
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.GameAdapter
import org.yuzu.yuzu_emu.databinding.FragmentGamesBinding
import org.yuzu.yuzu_emu.layout.AutofitGridLayoutManager
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.utils.ThemeHelper
import java.util.Locale

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

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        // Use custom back navigation so the user doesn't back out of the app when trying to back
        // out of the search view
        requireActivity().onBackPressedDispatcher.addCallback(
            viewLifecycleOwner,
            object : OnBackPressedCallback(true) {
                override fun handleOnBackPressed() {
                    if (binding.searchView.currentTransitionState == TransitionState.SHOWN) {
                        binding.searchView.hide()
                    } else {
                        requireActivity().finish()
                    }
                }
            })

        binding.gridGames.apply {
            layoutManager = AutofitGridLayoutManager(
                requireContext(),
                requireContext().resources.getDimensionPixelSize(R.dimen.card_width)
            )
            adapter = GameAdapter(requireActivity() as AppCompatActivity)
        }
        setUpSearch()

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

            if (!isReloading) {
                if (gamesViewModel.games.value!!.isEmpty()) {
                    binding.noticeText.visibility = View.VISIBLE
                } else {
                    binding.noticeText.visibility = View.GONE
                }
            }
        }
        gamesViewModel.games.observe(viewLifecycleOwner) {
            (binding.gridGames.adapter as GameAdapter).submitList(it)
        }
        gamesViewModel.searchedGames.observe(viewLifecycleOwner) {
            (binding.gridSearch.adapter as GameAdapter).submitList(it)
        }
        gamesViewModel.shouldSwapData.observe(viewLifecycleOwner) { shouldSwapData ->
            if (shouldSwapData) {
                (binding.gridGames.adapter as GameAdapter).submitList(gamesViewModel.games.value)
                gamesViewModel.setShouldSwapData(false)
            }
        }

        // Hide bottom navigation and FAB when using the search view
        binding.searchView.addTransitionListener { _: SearchView, _: TransitionState, newState: TransitionState ->
            when (newState) {
                TransitionState.SHOWING,
                TransitionState.SHOWN -> {
                    (binding.gridSearch.adapter as GameAdapter).submitList(emptyList())
                    searchShown()
                }
                TransitionState.HIDDEN,
                TransitionState.HIDING -> {
                    gamesViewModel.setSearchedGames(emptyList())
                    searchHidden()
                }
            }
        }

        // Ensure that bottom navigation or FAB don't appear upon recreation
        val searchState = binding.searchView.currentTransitionState
        if (searchState == TransitionState.SHOWN) {
            searchShown()
        } else if (searchState == TransitionState.HIDDEN) {
            searchHidden()
        }

        setInsets()

        // Make sure the loading indicator appears even if the layout is told to refresh before being fully drawn
        binding.swipeRefresh.post {
            binding.swipeRefresh.isRefreshing = gamesViewModel.isReloading.value!!
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun searchShown() {
        homeViewModel.setNavigationVisible(false)
        requireActivity().window.statusBarColor =
            ContextCompat.getColor(requireContext(), android.R.color.transparent)
    }

    private fun searchHidden() {
        homeViewModel.setNavigationVisible(true)
        requireActivity().window.statusBarColor = ThemeHelper.getColorWithOpacity(
            MaterialColors.getColor(
                binding.root,
                R.attr.colorSurface
            ), ThemeHelper.SYSTEM_BAR_ALPHA
        )
    }

    private inner class ScoredGame(val score: Double, val item: Game)

    private fun setUpSearch() {
        binding.gridSearch.apply {
            layoutManager = AutofitGridLayoutManager(
                requireContext(),
                requireContext().resources.getDimensionPixelSize(R.dimen.card_width)
            )
            adapter = GameAdapter(requireActivity() as AppCompatActivity)
        }

        binding.searchView.editText.doOnTextChanged { text: CharSequence?, _: Int, _: Int, _: Int ->
            val searchTerm = text.toString().lowercase(Locale.getDefault())
            val searchAlgorithm = Jaccard(2)
            val sortedList: List<Game> = gamesViewModel.games.value!!.mapNotNull { game ->
                val title = game.title.lowercase(Locale.getDefault())
                val score = searchAlgorithm.similarity(searchTerm, title)
                if (score > 0.03) {
                    ScoredGame(score, game)
                } else {
                    null
                }
            }.sortedByDescending { it.score }.map { it.item }
            gamesViewModel.setSearchedGames(sortedList)
        }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.gridGames) { view: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val extraListSpacing = resources.getDimensionPixelSize(R.dimen.spacing_med)

            view.setPadding(
                insets.left,
                insets.top + resources.getDimensionPixelSize(R.dimen.spacing_search),
                insets.right,
                insets.bottom + resources.getDimensionPixelSize(R.dimen.spacing_navigation) + extraListSpacing
            )
            binding.gridSearch.updatePadding(
                left = insets.left,
                top = extraListSpacing,
                right = insets.right,
                bottom = insets.bottom + extraListSpacing
            )

            binding.swipeRefresh.setSlingshotDistance(
                resources.getDimensionPixelSize(R.dimen.spacing_refresh_slingshot)
            )
            binding.swipeRefresh.setProgressViewOffset(
                false,
                insets.top + resources.getDimensionPixelSize(R.dimen.spacing_refresh_start),
                insets.top + resources.getDimensionPixelSize(R.dimen.spacing_refresh_end)
            )

            windowInsets
        }
}
