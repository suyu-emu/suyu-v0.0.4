// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.Manifest
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.isVisible
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.findNavController
import androidx.preference.PreferenceManager
import androidx.viewpager2.widget.ViewPager2.OnPageChangeCallback
import com.google.android.material.transition.MaterialFadeThrough
import org.yuzu.yuzu_emu.NativeLibrary
import java.io.File
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.adapters.SetupAdapter
import org.yuzu.yuzu_emu.databinding.FragmentSetupBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.model.ButtonState
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.model.PageButton
import org.yuzu.yuzu_emu.model.SetupCallback
import org.yuzu.yuzu_emu.model.SetupPage
import org.yuzu.yuzu_emu.model.PageState
import org.yuzu.yuzu_emu.ui.main.MainActivity
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.NativeConfig
import org.yuzu.yuzu_emu.utils.ViewUtils
import org.yuzu.yuzu_emu.utils.ViewUtils.setVisible
import org.yuzu.yuzu_emu.utils.collect

class SetupFragment : Fragment() {
    private var _binding: FragmentSetupBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()
    private val gamesViewModel: GamesViewModel by activityViewModels()

    private lateinit var mainActivity: MainActivity

    private lateinit var hasBeenWarned: BooleanArray

    private lateinit var pages: MutableList<SetupPage>

    private lateinit var pageButtonCallback: SetupCallback

    companion object {
        const val KEY_NEXT_VISIBILITY = "NextButtonVisibility"
        const val KEY_BACK_VISIBILITY = "BackButtonVisibility"
        const val KEY_HAS_BEEN_WARNED = "HasBeenWarned"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        exitTransition = MaterialFadeThrough()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSetupBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        mainActivity = requireActivity() as MainActivity

        requireActivity().onBackPressedDispatcher.addCallback(
            viewLifecycleOwner,
            object : OnBackPressedCallback(true) {
                override fun handleOnBackPressed() {
                    if (binding.viewPager2.currentItem > 0) {
                        pageBackward()
                    } else {
                        requireActivity().finish()
                    }
                }
            }
        )

        requireActivity().window.navigationBarColor =
            ContextCompat.getColor(requireContext(), android.R.color.transparent)

        pages = mutableListOf<SetupPage>()
        pages.apply {
            add(
                SetupPage(
                    R.drawable.ic_permission,
                    R.string.permissions,
                    R.string.permissions_description,
                    mutableListOf<PageButton>().apply {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            add(
                                PageButton(
                                    R.drawable.ic_notification,
                                    R.string.notifications,
                                    R.string.notifications_description,
                                    {
                                        pageButtonCallback = it
                                        permissionLauncher.launch(
                                            Manifest.permission.POST_NOTIFICATIONS
                                        )
                                    },
                                    {
                                        if (NotificationManagerCompat.from(requireContext())
                                                .areNotificationsEnabled()
                                        ) {
                                            ButtonState.BUTTON_ACTION_COMPLETE
                                        } else {
                                            ButtonState.BUTTON_ACTION_INCOMPLETE
                                        }
                                    },
                                    false,
                                    false,
                                )
                            )
                        }
                    },
                    {
                        if (NotificationManagerCompat.from(requireContext())
                                .areNotificationsEnabled()
                        ) {
                            PageState.COMPLETE
                        } else {
                            PageState.INCOMPLETE
                        }
                    }
                )
            )
            add(
                SetupPage(
                    R.drawable.ic_folder_open,
                    R.string.emulator_data,
                    R.string.emulator_data_description,
                    mutableListOf<PageButton>().apply {
                        add(
                            PageButton(
                                R.drawable.ic_key,
                                R.string.keys,
                                R.string.keys_description,
                                {
                                    pageButtonCallback = it
                                    getProdKey.launch(arrayOf("*/*"))
                                },
                                {
                                    val file = File(
                                        DirectoryInitialization.userDirectory + "/keys/prod.keys"
                                    )
                                    if (file.exists() && NativeLibrary.areKeysPresent()) {
                                        ButtonState.BUTTON_ACTION_COMPLETE
                                    } else {
                                        ButtonState.BUTTON_ACTION_INCOMPLETE
                                    }
                                },
                                false,
                                true,
                                R.string.install_prod_keys_warning,
                                R.string.install_prod_keys_warning_description,
                                R.string.install_prod_keys_warning_help,
                            )
                        )
                        add(
                            PageButton(
                                R.drawable.ic_firmware,
                                R.string.firmware,
                                R.string.firmware_description,
                                {
                                    pageButtonCallback = it
                                    getFirmware.launch(arrayOf("application/zip"))
                                },
                                {
                                    if (NativeLibrary.isFirmwareAvailable()) {
                                        ButtonState.BUTTON_ACTION_COMPLETE
                                    } else {
                                        ButtonState.BUTTON_ACTION_INCOMPLETE
                                    }
                                },
                                false,
                                true,
                                R.string.install_firmware_warning,
                                R.string.install_firmware_warning_description,
                                R.string.install_firmware_warning_help,
                            )
                        )
                        add(
                            PageButton(
                                R.drawable.ic_controller,
                                R.string.games,
                                R.string.games_description,
                                {
                                    pageButtonCallback = it
                                    getGamesDirectory.launch(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).data)
                                },
                                {
                                    if (NativeConfig.getGameDirs().isNotEmpty()) {
                                        ButtonState.BUTTON_ACTION_COMPLETE
                                    } else {
                                        ButtonState.BUTTON_ACTION_INCOMPLETE
                                    }
                                },
                                false,
                                true,
                                R.string.add_games_warning,
                                R.string.add_games_warning_description,
                                R.string.add_games_warning_help,
                            )
                        )
                    },
                    {
                        val file = File(
                            DirectoryInitialization.userDirectory + "/keys/prod.keys"
                        )
                        if (file.exists() && NativeLibrary.areKeysPresent() &&
                            NativeLibrary.isFirmwareAvailable() && NativeConfig.getGameDirs()
                                .isNotEmpty()
                        ) {
                            PageState.COMPLETE
                        } else {
                            PageState.INCOMPLETE
                        }
                    }
                )
            )
            add(
                SetupPage(
                    R.drawable.ic_check,
                    R.string.done,
                    R.string.done_description,
                    mutableListOf<PageButton>().apply {
                        add(
                            PageButton(
                                R.drawable.ic_arrow_forward,
                                R.string.get_started,
                                0,
                                buttonAction = {
                                    finishSetup()
                                },
                                buttonState = {
                                    ButtonState.BUTTON_ACTION_UNDEFINED
                                },
                            )
                        )
                    }
                ) { PageState.UNDEFINED }
            )
        }

        homeViewModel.shouldPageForward.collect(
            viewLifecycleOwner,
            resetState = { homeViewModel.setShouldPageForward(false) }
        ) { if (it) pageForward() }
        homeViewModel.gamesDirSelected.collect(
            viewLifecycleOwner,
            resetState = { homeViewModel.setGamesDirSelected(false) }
        ) { if (it) checkForButtonState.invoke() }

        binding.viewPager2.apply {
            adapter = SetupAdapter(requireActivity() as AppCompatActivity, pages)
            offscreenPageLimit = 2
            isUserInputEnabled = false
        }

        binding.viewPager2.registerOnPageChangeCallback(object : OnPageChangeCallback() {
            var previousPosition: Int = 0

            override fun onPageSelected(position: Int) {
                super.onPageSelected(position)

                val isFirstPage = position == 0
                val isLastPage = position == pages.size - 1

                if (isFirstPage) {
                    ViewUtils.hideView(binding.buttonBack)
                } else {
                    ViewUtils.showView(binding.buttonBack)
                }

                if (isLastPage) {
                    ViewUtils.hideView(binding.buttonNext)
                } else {
                    ViewUtils.showView(binding.buttonNext)
                }

                previousPosition = position
            }
        })

        binding.buttonNext.setOnClickListener {
            val index = binding.viewPager2.currentItem
            val currentPage = pages[index]

            val warningMessages =
                mutableListOf<Triple<Int, Int, Int>>() // title, description, helpLink

            currentPage.pageButtons?.forEach { button ->
                if (button.hasWarning || button.isUnskippable) {
                    val buttonState = button.buttonState()
                    if (buttonState == ButtonState.BUTTON_ACTION_COMPLETE) {
                        return@forEach
                    }

                    if (button.isUnskippable) {
                        MessageDialogFragment.newInstance(
                            activity = requireActivity(),
                            titleId = button.warningTitleId,
                            descriptionId = button.warningDescriptionId,
                            helpLinkId = button.warningHelpLinkId
                        ).show(childFragmentManager, MessageDialogFragment.TAG)
                        return@setOnClickListener
                    }

                    if (!hasBeenWarned[index]) {
                        warningMessages.add(
                            Triple(
                                button.warningTitleId,
                                button.warningDescriptionId,
                                button.warningHelpLinkId
                            )
                        )
                    }
                }
            }

            if (warningMessages.isNotEmpty()) {
                SetupWarningDialogFragment.newInstance(
                    warningMessages.map { it.first }.toIntArray(),
                    warningMessages.map { it.second }.toIntArray(),
                    warningMessages.map { it.third }.toIntArray(),
                    index
                ).show(childFragmentManager, SetupWarningDialogFragment.TAG)
                return@setOnClickListener
            }
            pageForward()
        }
        binding.buttonBack.setOnClickListener { pageBackward() }


        if (savedInstanceState != null) {
            val nextIsVisible = savedInstanceState.getBoolean(KEY_NEXT_VISIBILITY)
            val backIsVisible = savedInstanceState.getBoolean(KEY_BACK_VISIBILITY)
            hasBeenWarned = savedInstanceState.getBooleanArray(KEY_HAS_BEEN_WARNED)!!

            if (nextIsVisible) {
                binding.buttonNext.visibility = View.VISIBLE
            }
            if (backIsVisible) {
                binding.buttonBack.visibility = View.VISIBLE
            }
        } else {
            hasBeenWarned = BooleanArray(pages.size)
        }

        setInsets()
    }


    override fun onStop() {
        super.onStop()
        NativeConfig.saveGlobalConfig()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putBoolean(KEY_NEXT_VISIBILITY, binding.buttonNext.isVisible)
        outState.putBoolean(KEY_BACK_VISIBILITY, binding.buttonBack.isVisible)
        outState.putBooleanArray(KEY_HAS_BEEN_WARNED, hasBeenWarned)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private val checkForButtonState: () -> Unit = {
        val page = pages[binding.viewPager2.currentItem]
        page.pageButtons?.forEach {
            if (it.buttonState() == ButtonState.BUTTON_ACTION_COMPLETE) {
                pageButtonCallback.onStepCompleted(
                    it.titleId,
                    pageFullyCompleted = false
                )
            }

            if (page.pageSteps() == PageState.COMPLETE) {
                pageButtonCallback.onStepCompleted(0, pageFullyCompleted = true)
            }
        }
    }

    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) {
            if (it) {
                checkForButtonState.invoke()
            }

            if (!it &&
                !shouldShowRequestPermissionRationale(Manifest.permission.POST_NOTIFICATIONS)
            ) {
                PermissionDeniedDialogFragment().show(
                    childFragmentManager,
                    PermissionDeniedDialogFragment.TAG
                )
            }
        }


    val getProdKey =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result != null) {
                mainActivity.processKey(result, "keys")
                if (NativeLibrary.areKeysPresent()) {
                    checkForButtonState.invoke()
                }
            }
        }

    val getFirmware =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result != null) {
                mainActivity.processFirmware(result) {
                    if (NativeLibrary.isFirmwareAvailable()) {
                        checkForButtonState.invoke()
                    }
                }
            }
        }

    val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result != null) {
                mainActivity.processGamesDir(result)
            }
        }

    private fun finishSetup() {
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
            .edit()
            .putBoolean(Settings.PREF_FIRST_APP_LAUNCH, false)
            .apply()

        gamesViewModel.reloadGames(directoriesChanged = true, firstStartup = false)

        mainActivity.finishSetup(binding.root.findNavController())
    }

    fun pageForward() {
        if (_binding != null) {
            binding.viewPager2.currentItem += 1
        }
    }

    fun pageBackward() {
        if (_binding != null) {
            binding.viewPager2.currentItem -= 1
        }
    }

    fun setPageWarned(page: Int) {
        hasBeenWarned[page] = true
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets =
                windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets =
                windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftPadding = barInsets.left + cutoutInsets.left
            val topPadding = barInsets.top + cutoutInsets.top
            val rightPadding = barInsets.right + cutoutInsets.right
            val bottomPadding = barInsets.bottom + cutoutInsets.bottom

            if (resources.getBoolean(R.bool.small_layout)) {
                binding.viewPager2
                    .updatePadding(
                        left = leftPadding,
                        top = topPadding,
                        right = rightPadding
                    )
                binding.constraintButtons
                    .updatePadding(
                        left = leftPadding,
                        right = rightPadding,
                        bottom = bottomPadding
                    )
            } else {
                binding.viewPager2.updatePadding(
                    top = topPadding,
                    bottom = bottomPadding
                )
                binding.constraintButtons
                    .updatePadding(
                        left = leftPadding,
                        right = rightPadding,
                        bottom = bottomPadding
                    )
            }
            windowInsets
        }
}
