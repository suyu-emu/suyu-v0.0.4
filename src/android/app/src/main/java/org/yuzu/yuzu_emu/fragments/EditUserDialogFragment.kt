// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Activity
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.ImageDecoder
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.MediaStore
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.core.widget.doAfterTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.FirmwareAvatarAdapter
import org.yuzu.yuzu_emu.databinding.FragmentEditUserDialogBinding
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.model.ProfileUtils
import org.yuzu.yuzu_emu.model.UserProfile
import java.io.File
import java.io.FileOutputStream
import androidx.core.graphics.scale
import androidx.core.graphics.createBitmap

class EditUserDialogFragment : Fragment() {
    private var _binding: FragmentEditUserDialogBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()

    private var currentUUID: String = ""
    private var isEditMode = false
    private var selectedImageUri: Uri? = null
    private var selectedFirmwareAvatar: Bitmap? = null
    private var hasCustomImage = false
    private var revertedToDefault = false

    companion object {
        private const val ARG_UUID = "uuid"
        private const val ARG_USERNAME = "username"

        fun newInstance(profile: UserProfile?): EditUserDialogFragment {
            val fragment = EditUserDialogFragment()
            profile?.let {
                val args = Bundle()
                args.putString(ARG_UUID, it.uuid)
                args.putString(ARG_USERNAME, it.username)
                fragment.arguments = args
            }
            return fragment
        }
    }

    private val imagePickerLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri ->
                selectedImageUri = uri
                loadImage(uri)
                hasCustomImage = true
                binding.buttonRevertImage.visibility = View.VISIBLE
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentEditUserDialogBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        homeViewModel.setStatusBarShadeVisibility(visible = false)

        val existingUUID = arguments?.getString(ARG_UUID)
        val existingUsername = arguments?.getString(ARG_USERNAME)

        if (existingUUID != null && existingUsername != null) {
            isEditMode = true
            currentUUID = existingUUID
            binding.toolbarNewUser.title = getString(R.string.profile_edit_user)
            binding.editUsername.setText(existingUsername)
            binding.textUuid.text = formatUUID(existingUUID)
            binding.buttonGenerateUuid.visibility = View.GONE

            val imagePath = NativeLibrary.getUserImagePath(existingUUID)
            val imageFile = File(imagePath)
            if (imageFile.exists()) {
                val bitmap = BitmapFactory.decodeFile(imagePath)
                binding.imageUserAvatar.setImageBitmap(bitmap)
                hasCustomImage = true
                binding.buttonRevertImage.visibility = View.VISIBLE
            } else {
                loadDefaultAvatar()
            }
        } else {
            isEditMode = false
            currentUUID = ProfileUtils.generateRandomUUID()
            binding.toolbarNewUser.title = getString(R.string.profile_new_user)
            binding.textUuid.text = formatUUID(currentUUID)
            loadDefaultAvatar()
        }

        binding.toolbarNewUser.setNavigationOnClickListener {
            findNavController().popBackStack()
        }

        binding.editUsername.doAfterTextChanged {
            validateInput()
        }

        binding.buttonGenerateUuid.setOnClickListener {
            currentUUID = ProfileUtils.generateRandomUUID()
            binding.textUuid.text = formatUUID(currentUUID)
        }

        binding.buttonSelectImage.setOnClickListener {
            selectImage()
        }

        binding.buttonRevertImage.setOnClickListener {
            revertToDefaultImage()
        }

        if (NativeLibrary.isFirmwareAvailable()) {
            binding.buttonFirmwareAvatars.visibility = View.VISIBLE
            binding.buttonFirmwareAvatars.setOnClickListener {
                showFirmwareAvatarPicker()
            }
        }

        binding.buttonSave.setOnClickListener {
            saveUser()
        }

        binding.buttonCancel.setOnClickListener {
            findNavController().popBackStack()
        }

        validateInput()
        setInsets()
    }

    private fun showFirmwareAvatarPicker() {
        val dialogView = LayoutInflater.from(requireContext())
            .inflate(R.layout.dialog_firmware_avatar_picker, null)

        val gridAvatars = dialogView.findViewById<RecyclerView>(R.id.grid_avatars)
        val progressLoading = dialogView.findViewById<View>(R.id.progress_loading)
        val textEmpty = dialogView.findViewById<View>(R.id.text_empty)

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.profile_firmware_avatars)
            .setView(dialogView)
            .setNegativeButton(android.R.string.cancel, null)
            .create()

        dialog.show()

        viewLifecycleOwner.lifecycleScope.launch {
            val avatars = withContext(Dispatchers.IO) {
                loadFirmwareAvatars()
            }

            if (avatars.isEmpty()) {
                progressLoading.visibility = View.GONE
                textEmpty.visibility = View.VISIBLE
            } else {
                progressLoading.visibility = View.GONE
                gridAvatars.visibility = View.VISIBLE

                val adapter = FirmwareAvatarAdapter(avatars) { selectedAvatar ->
                    val scaledBitmap = selectedAvatar.scale(256, 256)
                    binding.imageUserAvatar.setImageBitmap(scaledBitmap)
                    selectedFirmwareAvatar = scaledBitmap
                    hasCustomImage = true
                    binding.buttonRevertImage.visibility = View.VISIBLE
                    dialog.dismiss()
                }

                gridAvatars.apply {
                    layoutManager = GridLayoutManager(requireContext(), 4)
                    this.adapter = adapter
                }
            }
        }
    }

    private fun loadFirmwareAvatars(): List<Bitmap> {
        val avatars = mutableListOf<Bitmap>()
        val count = NativeLibrary.getFirmwareAvatarCount()

        for (i in 0 until count) {
            try {
                val imageData = NativeLibrary.getFirmwareAvatarImage(i) ?: continue

                val argbData = IntArray(256 * 256)
                for (pixel in 0 until 256 * 256) {
                    val offset = pixel * 4
                    val r = imageData[offset].toInt() and 0xFF
                    val g = imageData[offset + 1].toInt() and 0xFF
                    val b = imageData[offset + 2].toInt() and 0xFF
                    val a = imageData[offset + 3].toInt() and 0xFF
                    argbData[pixel] = (a shl 24) or (r shl 16) or (g shl 8) or b
                }

                val bitmap = Bitmap.createBitmap(argbData, 256, 256, Bitmap.Config.ARGB_8888)
                avatars.add(bitmap)
            } catch (e: Exception) {
                continue
            }
        }

        return avatars
    }

    private fun formatUUID(uuid: String): String {
        if (uuid.length != 32) return uuid
        return buildString {
            append(uuid.substring(0, 8))
            append("-")
            append(uuid.substring(8, 12))
            append("-")
            append(uuid.substring(12, 16))
            append("-")
            append(uuid.substring(16, 20))
            append("-")
            append(uuid.substring(20, 32))
        }
    }

    private fun validateInput() {
        val username = binding.editUsername.text.toString()
        val isValid = username.isNotEmpty() && username.length <= 32
        binding.buttonSave.isEnabled = isValid
    }

    private fun selectImage() {
        val intent = Intent(Intent.ACTION_PICK).apply {
            type = "image/*"
        }
        imagePickerLauncher.launch(intent)
    }

    private fun loadImage(uri: Uri) {
        try {
            val bitmap = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                val source = ImageDecoder.createSource(requireContext().contentResolver, uri)
                ImageDecoder.decodeBitmap(source) { decoder, _, _ ->
                    decoder.setTargetSampleSize(1)
                }
            } else {
                @Suppress("DEPRECATION")
                MediaStore.Images.Media.getBitmap(requireContext().contentResolver, uri)
            }

            val croppedBitmap = centerCropBitmap(bitmap, 256, 256)
            binding.imageUserAvatar.setImageBitmap(croppedBitmap)
        } catch (e: Exception) {
            MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.error)
                .setMessage(getString(R.string.profile_image_load_error, e.message))
                .setPositiveButton(android.R.string.ok, null)
                .show()
        }
    }

    private fun loadDefaultAvatar() {
        val jpegData = NativeLibrary.getDefaultAccountBackupJpeg()
        val bitmap = BitmapFactory.decodeByteArray(jpegData, 0, jpegData.size)
        binding.imageUserAvatar.setImageBitmap(bitmap)

        hasCustomImage = false
        binding.buttonRevertImage.visibility = View.GONE
    }

    private fun revertToDefaultImage() {
        selectedImageUri = null
        selectedFirmwareAvatar = null
        revertedToDefault = true
        loadDefaultAvatar()
    }

    private fun saveUser() {
        val username = binding.editUsername.text.toString()

        if (isEditMode) {
            if (NativeLibrary.updateUserUsername(currentUUID, username)) {
                saveImageIfNeeded()
                findNavController().popBackStack()
            } else {
                showError(getString(R.string.profile_update_failed))
            }
        } else {
            if (NativeLibrary.createUser(currentUUID, username)) {
                saveImageIfNeeded()
                findNavController().popBackStack()
            } else {
                showError(getString(R.string.profile_create_failed))
            }
        }
    }

    private fun saveImageIfNeeded() {
        if (revertedToDefault && isEditMode) {
            val imagePath = NativeLibrary.getUserImagePath(currentUUID)
            if (imagePath != null) {
                val imageFile = File(imagePath)
                if (imageFile.exists()) {
                    imageFile.delete()
                }
            }

            return
        }

        if (!hasCustomImage) {
            return
        }

        try {
            val bitmapToSave: Bitmap? = when {
                selectedFirmwareAvatar != null -> selectedFirmwareAvatar
                selectedImageUri != null -> {
                    val bitmap = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        val source = ImageDecoder.createSource(
                            requireContext().contentResolver,
                            selectedImageUri!!
                        )
                        ImageDecoder.decodeBitmap(source)
                    } else {
                        @Suppress("DEPRECATION")
                        MediaStore.Images.Media.getBitmap(
                            requireContext().contentResolver,
                            selectedImageUri
                        )
                    }
                    centerCropBitmap(bitmap, 256, 256)
                }

                else -> null
            }

            if (bitmapToSave == null) {
                return
            }

            val tempFile = File(requireContext().cacheDir, "temp_avatar_${currentUUID}.jpg")
            FileOutputStream(tempFile).use { out ->
                bitmapToSave.compress(Bitmap.CompressFormat.JPEG, 100, out)
            }

            NativeLibrary.saveUserImage(currentUUID, tempFile.absolutePath)

            tempFile.delete()
        } catch (e: Exception) {
            showError(getString(R.string.profile_image_save_error, e.message))
        }
    }

    private fun centerCropBitmap(source: Bitmap, targetWidth: Int, targetHeight: Int): Bitmap {
        val sourceWidth = source.width
        val sourceHeight = source.height

        val scale = maxOf(
            targetWidth.toFloat() / sourceWidth,
            targetHeight.toFloat() / sourceHeight
        )

        val scaledWidth = (sourceWidth * scale).toInt()
        val scaledHeight = (sourceHeight * scale).toInt()

        val scaledBitmap = source.scale(scaledWidth, scaledHeight)

        val x = (scaledWidth - targetWidth) / 2
        val y = (scaledHeight - targetHeight) / 2

        return Bitmap.createBitmap(scaledBitmap, x, y, targetWidth, targetHeight)
    }

    private fun showError(message: String) {
        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.error)
            .setMessage(message)
            .setPositiveButton(android.R.string.ok, null)
            .show()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInset = barInsets.left + cutoutInsets.left
            val topInset =  cutoutInsets.top
            val rightInset = barInsets.right + cutoutInsets.right
            val bottomInset = barInsets.bottom + cutoutInsets.bottom

            binding.appbar.updatePadding(
                left = leftInset,
                top = topInset,
                right = rightInset
            )

            binding.scrollContent.updatePadding(
                left = leftInset,
                right = rightInset
            )

            binding.buttonContainer.updatePadding(
                left = leftInset,
                right = rightInset,
                bottom = bottomInset
            )

            windowInsets
        }



    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
