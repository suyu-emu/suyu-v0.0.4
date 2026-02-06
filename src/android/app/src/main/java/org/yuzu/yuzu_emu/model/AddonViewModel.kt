// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.util.concurrent.atomic.AtomicBoolean

class AddonViewModel : ViewModel() {
    private val _patchList = MutableStateFlow(mutableListOf<Patch>())
    val addonList get() = _patchList.asStateFlow()

    private val _showModInstallPicker = MutableStateFlow(false)
    val showModInstallPicker get() = _showModInstallPicker.asStateFlow()

    private val _showModNoticeDialog = MutableStateFlow(false)
    val showModNoticeDialog get() = _showModNoticeDialog.asStateFlow()

    private val _addonToDelete = MutableStateFlow<Patch?>(null)
    val addonToDelete = _addonToDelete.asStateFlow()

    var game: Game? = null

    private val isRefreshing = AtomicBoolean(false)

    fun onOpenAddons(game: Game) {
        this.game = game
        refreshAddons()
    }

    fun refreshAddons() {
        if (isRefreshing.get() || game == null) {
            return
        }
        isRefreshing.set(true)
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                val patchList = (
                    NativeLibrary.getPatchesForFile(game!!.path, game!!.programId)
                        ?: emptyArray()
                    ).toMutableList()
                patchList.sortBy { it.name }

                // Ensure only one update is enabled
                ensureSingleUpdateEnabled(patchList)

                removeDuplicates(patchList)

                _patchList.value = patchList
                isRefreshing.set(false)
            }
        }
    }

    private fun ensureSingleUpdateEnabled(patchList: MutableList<Patch>) {
        val updates = patchList.filter { PatchType.from(it.type) == PatchType.Update }
        if (updates.size <= 1) {
            return
        }

        val enabledUpdates = updates.filter { it.enabled }

        if (enabledUpdates.size > 1) {
            val nandOrSdmcEnabled = enabledUpdates.find {
                it.name.contains("(NAND)") || it.name.contains("(SDMC)")
            }

            val updateToKeep = nandOrSdmcEnabled ?: enabledUpdates.first()

            for (patch in patchList) {
                if (PatchType.from(patch.type) == PatchType.Update) {
                    patch.enabled = (patch === updateToKeep)
                }
            }
        }
    }

    private fun removeDuplicates(patchList: MutableList<Patch>) {
        val seen = mutableSetOf<String>()
        val iterator = patchList.iterator()
        while (iterator.hasNext()) {
            val patch = iterator.next()
            val key = "${patch.name}|${patch.version}|${patch.type}"
            if (seen.contains(key)) {
                iterator.remove()
            } else {
                seen.add(key)
            }
        }
    }

    fun setAddonToDelete(patch: Patch?) {
        _addonToDelete.value = patch
    }

    fun enableOnlyThisUpdate(selectedPatch: Patch) {
        val currentList = _patchList.value
        for (patch in currentList) {
            if (PatchType.from(patch.type) == PatchType.Update) {
                patch.enabled = (patch === selectedPatch)
            }
        }
    }

    fun onDeleteAddon(patch: Patch) {
        when (PatchType.from(patch.type)) {
            PatchType.Update -> NativeLibrary.removeUpdate(patch.programId)
            PatchType.DLC -> NativeLibrary.removeDLC(patch.programId)
            PatchType.Mod -> NativeLibrary.removeMod(patch.programId, patch.name)
        }
        refreshAddons()
    }

    fun onCloseAddons() {
        if (_patchList.value.isEmpty()) {
            return
        }

        // Check if there are multiple update versions
        val updates = _patchList.value.filter { PatchType.from(it.type) == PatchType.Update }
        val hasMultipleUpdates = updates.size > 1

        NativeConfig.setDisabledAddons(
            game!!.programId,
            _patchList.value.mapNotNull {
                if (it.enabled) {
                    null
                } else {
                    if (PatchType.from(it.type) == PatchType.Update) {
                        if (it.name.contains("(NAND)") || it.name.contains("(SDMC)")) {
                            it.name
                        } else if (hasMultipleUpdates) {
                            "Update@${it.numericVersion}"
                        } else {
                            it.name
                        }
                    } else {
                        it.name
                    }
                }
            }.toTypedArray()
        )
        NativeConfig.saveGlobalConfig()
        _patchList.value.clear()
        game = null
    }

    fun showModInstallPicker(install: Boolean) {
        _showModInstallPicker.value = install
    }

    fun showModNoticeDialog(show: Boolean) {
        _showModNoticeDialog.value = show
    }
}
