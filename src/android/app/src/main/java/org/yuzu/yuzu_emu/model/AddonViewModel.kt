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
    private val _addonList = MutableStateFlow(mutableListOf<Addon>())
    val addonList get() = _addonList.asStateFlow()

    private val _showModInstallPicker = MutableStateFlow(false)
    val showModInstallPicker get() = _showModInstallPicker.asStateFlow()

    private val _showModNoticeDialog = MutableStateFlow(false)
    val showModNoticeDialog get() = _showModNoticeDialog.asStateFlow()

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
                val addonList = mutableListOf<Addon>()
                val disabledAddons = NativeConfig.getDisabledAddons(game!!.programId)
                NativeLibrary.getAddonsForFile(game!!.path, game!!.programId)?.forEach {
                    val name = it.first.replace("[D] ", "")
                    addonList.add(Addon(!disabledAddons.contains(name), name, it.second))
                }
                addonList.sortBy { it.title }
                _addonList.value = addonList
                isRefreshing.set(false)
            }
        }
    }

    fun onCloseAddons() {
        if (_addonList.value.isEmpty()) {
            return
        }

        NativeConfig.setDisabledAddons(
            game!!.programId,
            _addonList.value.mapNotNull {
                if (it.enabled) {
                    null
                } else {
                    it.title
                }
            }.toTypedArray()
        )
        NativeConfig.saveGlobalConfig()
        _addonList.value.clear()
        game = null
    }

    fun showModInstallPicker(install: Boolean) {
        _showModInstallPicker.value = install
    }

    fun showModNoticeDialog(show: Boolean) {
        _showModNoticeDialog.value = show
    }
}
