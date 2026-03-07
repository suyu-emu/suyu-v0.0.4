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
    private val _patchList = MutableStateFlow<List<Patch>>(emptyList())
    val addonList get() = _patchList.asStateFlow()

    private val _showModInstallPicker = MutableStateFlow(false)
    val showModInstallPicker get() = _showModInstallPicker.asStateFlow()

    private val _showModNoticeDialog = MutableStateFlow(false)
    val showModNoticeDialog get() = _showModNoticeDialog.asStateFlow()

    private val _addonToDelete = MutableStateFlow<Patch?>(null)
    val addonToDelete = _addonToDelete.asStateFlow()

    var game: Game? = null
    private var loadedGameKey: String? = null

    private val isRefreshing = AtomicBoolean(false)
    private val pendingRefresh = AtomicBoolean(false)

    fun onAddonsViewCreated(game: Game) {
        this.game = game
        refreshAddons(commitEmpty = false)
    }

    fun onAddonsViewStarted(game: Game) {
        this.game = game
        val hasLoadedCurrentGame = loadedGameKey == gameKey(game)
        refreshAddons(force = !hasLoadedCurrentGame)
    }

    fun refreshAddons(force: Boolean = false, commitEmpty: Boolean = true) {
        val currentGame = game ?: return
        val currentGameKey = gameKey(currentGame)
        if (!force && loadedGameKey == currentGameKey) {
            return
        }
        if (!isRefreshing.compareAndSet(false, true)) {
            if (force) {
                pendingRefresh.set(true)
            }
            return
        }

        viewModelScope.launch {
            try {
                val patches = withContext(Dispatchers.IO) {
                    NativeLibrary.getPatchesForFile(currentGame.path, currentGame.programId)
                } ?: return@launch

                val patchList = patches.toMutableList()
                patchList.sortBy { it.name }

                // Ensure only one update is enabled
                ensureSingleUpdateEnabled(patchList)

                removeDuplicates(patchList)
                if (patchList.isEmpty() && !commitEmpty) {
                    return@launch
                }
                if (gameKey(game ?: return@launch) != currentGameKey) {
                    return@launch
                }

                _patchList.value = patchList.toList()
                loadedGameKey = currentGameKey
            } finally {
                isRefreshing.set(false)
                if (pendingRefresh.compareAndSet(true, false)) {
                    refreshAddons(force = true)
                }
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
        refreshAddons(force = true)
    }

    fun onCloseAddons() {
        val currentGame = game ?: run {
            _patchList.value = emptyList()
            loadedGameKey = null
            return
        }
        val currentList = _patchList.value
        if (currentList.isEmpty()) {
            _patchList.value = emptyList()
            loadedGameKey = null
            game = null
            return
        }

        NativeConfig.setDisabledAddons(
            currentGame.programId,
            currentList.mapNotNull {
                if (it.enabled) {
                    null
                } else {
                    if (PatchType.from(it.type) == PatchType.Update) {
                        if (it.name.contains("(NAND)") || it.name.contains("(SDMC)")) {
                            it.name
                        } else if (it.numericVersion != 0L) {
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
        _patchList.value = emptyList()
        loadedGameKey = null
        game = null
    }

    fun showModInstallPicker(install: Boolean) {
        _showModInstallPicker.value = install
    }

    fun showModNoticeDialog(show: Boolean) {
        _showModNoticeDialog.value = show
    }

    private fun gameKey(game: Game): String {
        return "${game.programId}|${game.path}"
    }
}
