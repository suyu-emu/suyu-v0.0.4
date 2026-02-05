// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.transition.MaterialSharedAxis
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.ProfileAdapter
import org.yuzu.yuzu_emu.databinding.FragmentProfileManagerBinding
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.model.UserProfile
import org.yuzu.yuzu_emu.utils.NativeConfig

class ProfileManagerFragment : Fragment() {
    private var _binding: FragmentProfileManagerBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()
    private lateinit var profileAdapter: ProfileAdapter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentProfileManagerBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        homeViewModel.setStatusBarShadeVisibility(visible = false)

        binding.toolbarProfiles.setNavigationOnClickListener {
            findNavController().popBackStack()
        }

        setupRecyclerView()
        loadProfiles()

        binding.buttonAddUser.setOnClickListener {
            if (NativeLibrary.canCreateUser()) {
                findNavController().navigate(R.id.action_profileManagerFragment_to_newUserDialog)
            } else {
                MaterialAlertDialogBuilder(requireContext())
                    .setTitle(R.string.profile_max_users_title)
                    .setMessage(R.string.profile_max_users_message)
                    .setPositiveButton(android.R.string.ok, null)
                    .show()
            }
        }

        setInsets()
    }

    override fun onResume() {
        super.onResume()
        loadProfiles()
    }

    private fun setupRecyclerView() {
        profileAdapter = ProfileAdapter(
            onProfileClick = { profile -> selectProfile(profile) },
            onEditClick = { profile -> editProfile(profile) },
            onDeleteClick = { profile -> confirmDeleteProfile(profile) }
        )
        binding.listProfiles.apply {
            layoutManager = LinearLayoutManager(requireContext())
            adapter = profileAdapter
        }
    }

    private fun loadProfiles() {
        val profiles = mutableListOf<UserProfile>()
        val userUUIDs = NativeLibrary.getAllUsers() ?: emptyArray()
        val currentUserUUID = NativeLibrary.getCurrentUser()

        for (uuid in userUUIDs) {
            if (uuid.isNotEmpty()) {
                val username = NativeLibrary.getUserUsername(uuid)
                if (!username.isNullOrEmpty()) {
                    val imagePath = NativeLibrary.getUserImagePath(uuid) ?: ""
                    profiles.add(UserProfile(uuid, username, imagePath))
                }
            }
        }

        profileAdapter.submitList(profiles)
        profileAdapter.setCurrentUser(currentUserUUID ?: "")

        binding.buttonAddUser.isEnabled = NativeLibrary.canCreateUser()
    }

    private fun selectProfile(profile: UserProfile) {
        if (NativeLibrary.setCurrentUser(profile.uuid)) {
            loadProfiles()
        }
    }


    private fun editProfile(profile: UserProfile) {
        val bundle = Bundle().apply {
            putString("uuid", profile.uuid)
            putString("username", profile.username)
        }
        findNavController().navigate(R.id.action_profileManagerFragment_to_newUserDialog, bundle)
    }

    private fun confirmDeleteProfile(profile: UserProfile) {
        val currentUser = NativeLibrary.getCurrentUser()
        val isCurrentUser = profile.uuid == currentUser

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.profile_delete_confirm_title)
            .setMessage(
                if (isCurrentUser) {
                    getString(R.string.profile_delete_current_user_message, profile.username)
                } else {
                    getString(R.string.profile_delete_confirm_message, profile.username)
                }
            )
            .setPositiveButton(R.string.profile_delete) { _, _ ->
                deleteProfile(profile)
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun deleteProfile(profile: UserProfile) {
        val currentUser = NativeLibrary.getCurrentUser()
        if (!currentUser.isNullOrEmpty() && profile.uuid == currentUser) {
            val users = NativeLibrary.getAllUsers() ?: emptyArray()
            for (uuid in users) {
                if (uuid.isNotEmpty() && uuid != profile.uuid) {
                    NativeLibrary.setCurrentUser(uuid)
                    break
                }
            }
        }

        if (NativeLibrary.removeUser(profile.uuid)) {
            loadProfiles()
        }
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val fabLayoutParams = binding.buttonAddUser.layoutParams as ViewGroup.MarginLayoutParams
            fabLayoutParams.leftMargin = leftInsets + 24
            fabLayoutParams.rightMargin = rightInsets + 24
            fabLayoutParams.bottomMargin = barInsets.bottom + 24
            binding.buttonAddUser.layoutParams = fabLayoutParams

            windowInsets
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        NativeConfig.saveGlobalConfig()
        _binding = null
    }
}
