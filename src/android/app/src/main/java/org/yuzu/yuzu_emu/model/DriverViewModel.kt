// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import org.yuzu.yuzu_emu.utils.GpuDriverMetadata
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.BufferedOutputStream
import java.io.File

class DriverViewModel : ViewModel() {
    private val _areDriversLoading = MutableStateFlow(false)
    private val _isDriverReady = MutableStateFlow(true)
    private val _isDeletingDrivers = MutableStateFlow(false)

    val isInteractionAllowed: StateFlow<Boolean> =
        combine(
            _areDriversLoading,
            _isDriverReady,
            _isDeletingDrivers
        ) { loading, ready, deleting ->
            !loading && ready && !deleting
        }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(), initialValue = false)

    private val _driverList = MutableStateFlow(GpuDriverHelper.getDrivers())
    val driverList: StateFlow<MutableList<Pair<String, GpuDriverMetadata>>> get() = _driverList

    var previouslySelectedDriver = 0
    var selectedDriver = -1

    // Used for showing which driver is currently installed within the driver manager card
    private val _selectedDriverTitle = MutableStateFlow("")
    val selectedDriverTitle: StateFlow<String> get() = _selectedDriverTitle

    private val _newDriverInstalled = MutableStateFlow(false)
    val newDriverInstalled: StateFlow<Boolean> get() = _newDriverInstalled

    val driversToDelete = mutableListOf<String>()

    init {
        val currentDriverMetadata = GpuDriverHelper.installedCustomDriverData
        findSelectedDriver(currentDriverMetadata)

        // If a user had installed a driver before the manager was implemented, this zips
        // the installed driver to UserData/gpu_drivers/CustomDriver.zip so that it can
        // be indexed and exported as expected.
        if (selectedDriver == -1) {
            val driverToSave =
                File(GpuDriverHelper.driverStoragePath, "CustomDriver.zip")
            driverToSave.createNewFile()
            FileUtil.zipFromInternalStorage(
                File(GpuDriverHelper.driverInstallationPath!!),
                GpuDriverHelper.driverInstallationPath!!,
                BufferedOutputStream(driverToSave.outputStream())
            )
            _driverList.value.add(Pair(driverToSave.path, currentDriverMetadata))
            setSelectedDriverIndex(_driverList.value.size - 1)
        }

        // If a user had installed a driver before the config was reworked to be multiplatform,
        // we have save the path of the previously selected driver to the new setting.
        if (StringSetting.DRIVER_PATH.getString(true).isEmpty() && selectedDriver > 0 &&
            StringSetting.DRIVER_PATH.global
        ) {
            StringSetting.DRIVER_PATH.setString(_driverList.value[selectedDriver].first)
            NativeConfig.saveGlobalConfig()
        } else {
            findSelectedDriver(GpuDriverHelper.customDriverSettingData)
        }
        updateDriverNameForGame(null)
    }

    fun setSelectedDriverIndex(value: Int) {
        if (selectedDriver != -1) {
            previouslySelectedDriver = selectedDriver
        }
        selectedDriver = value
    }

    fun setNewDriverInstalled(value: Boolean) {
        _newDriverInstalled.value = value
    }

    fun addDriver(driverData: Pair<String, GpuDriverMetadata>) {
        val driverIndex = _driverList.value.indexOfFirst { it == driverData }
        if (driverIndex == -1) {
            _driverList.value.add(driverData)
            setSelectedDriverIndex(_driverList.value.size - 1)
            _selectedDriverTitle.value = driverData.second.name
                ?: YuzuApplication.appContext.getString(R.string.system_gpu_driver)
        } else {
            setSelectedDriverIndex(driverIndex)
        }
    }

    fun removeDriver(driverData: Pair<String, GpuDriverMetadata>) {
        _driverList.value.remove(driverData)
    }

    fun onOpenDriverManager(game: Game?) {
        if (game != null) {
            SettingsFile.loadCustomConfig(game)
        }

        val driverPath = StringSetting.DRIVER_PATH.getString()
        if (driverPath.isEmpty()) {
            setSelectedDriverIndex(0)
        } else {
            findSelectedDriver(GpuDriverHelper.getMetadataFromZip(File(driverPath)))
        }
    }

    fun onCloseDriverManager(game: Game?) {
        _isDeletingDrivers.value = true
        StringSetting.DRIVER_PATH.setString(driverList.value[selectedDriver].first)
        updateDriverNameForGame(game)
        if (game == null) {
            NativeConfig.saveGlobalConfig()
        } else {
            NativeConfig.savePerGameConfig()
            NativeConfig.unloadPerGameConfig()
            NativeConfig.reloadGlobalConfig()
        }

        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                driversToDelete.forEach {
                    val driver = File(it)
                    if (driver.exists()) {
                        driver.delete()
                    }
                }
                driversToDelete.clear()
                _isDeletingDrivers.value = false
            }
        }
    }

    // It is the Emulation Fragment's responsibility to load per-game settings so that this function
    // knows what driver to load.
    fun onLaunchGame() {
        _isDriverReady.value = false

        val selectedDriverFile = File(StringSetting.DRIVER_PATH.getString())
        val selectedDriverMetadata = GpuDriverHelper.customDriverSettingData
        if (GpuDriverHelper.installedCustomDriverData == selectedDriverMetadata) {
            return
        }

        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                if (selectedDriverMetadata.name == null) {
                    GpuDriverHelper.installDefaultDriver()
                    setDriverReady()
                    return@withContext
                }

                if (selectedDriverFile.exists()) {
                    GpuDriverHelper.installCustomDriver(selectedDriverFile)
                } else {
                    GpuDriverHelper.installDefaultDriver()
                }
                setDriverReady()
            }
        }
    }

    private fun findSelectedDriver(currentDriverMetadata: GpuDriverMetadata) {
        if (driverList.value.size == 1) {
            setSelectedDriverIndex(0)
            return
        }

        driverList.value.forEachIndexed { i: Int, driver: Pair<String, GpuDriverMetadata> ->
            if (driver.second == currentDriverMetadata) {
                setSelectedDriverIndex(i)
                return
            }
        }
    }

    fun updateDriverNameForGame(game: Game?) {
        if (!GpuDriverHelper.supportsCustomDriverLoading()) {
            return
        }

        if (game == null || NativeConfig.isPerGameConfigLoaded()) {
            updateName()
        } else {
            SettingsFile.loadCustomConfig(game)
            updateName()
            NativeConfig.unloadPerGameConfig()
            NativeConfig.reloadGlobalConfig()
        }
    }

    private fun updateName() {
        _selectedDriverTitle.value = GpuDriverHelper.customDriverSettingData.name
            ?: YuzuApplication.appContext.getString(R.string.system_gpu_driver)
    }

    private fun setDriverReady() {
        _isDriverReady.value = true
        _selectedDriverTitle.value = GpuDriverHelper.customDriverSettingData.name
            ?: YuzuApplication.appContext.getString(R.string.system_gpu_driver)
    }
}
