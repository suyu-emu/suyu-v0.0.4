// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import org.yuzu.yuzu_emu.utils.GpuDriverMetadata
import java.io.BufferedOutputStream
import java.io.File

class DriverViewModel : ViewModel() {
    private val _areDriversLoading = MutableStateFlow(false)
    val areDriversLoading: StateFlow<Boolean> get() = _areDriversLoading

    private val _isDriverReady = MutableStateFlow(true)
    val isDriverReady: StateFlow<Boolean> get() = _isDriverReady

    private val _isDeletingDrivers = MutableStateFlow(false)
    val isDeletingDrivers: StateFlow<Boolean> get() = _isDeletingDrivers

    private val _driverList = MutableStateFlow(mutableListOf<Pair<String, GpuDriverMetadata>>())
    val driverList: StateFlow<MutableList<Pair<String, GpuDriverMetadata>>> get() = _driverList

    var previouslySelectedDriver = 0
    var selectedDriver = -1

    private val _selectedDriverMetadata =
        MutableStateFlow(
            GpuDriverHelper.customDriverData.name
                ?: YuzuApplication.appContext.getString(R.string.system_gpu_driver)
        )
    val selectedDriverMetadata: StateFlow<String> get() = _selectedDriverMetadata

    private val _newDriverInstalled = MutableStateFlow(false)
    val newDriverInstalled: StateFlow<Boolean> get() = _newDriverInstalled

    val driversToDelete = mutableListOf<String>()

    val isInteractionAllowed
        get() = !areDriversLoading.value && isDriverReady.value && !isDeletingDrivers.value

    init {
        _areDriversLoading.value = true
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                val drivers = GpuDriverHelper.getDrivers()
                val currentDriverMetadata = GpuDriverHelper.customDriverData
                for (i in drivers.indices) {
                    if (drivers[i].second == currentDriverMetadata) {
                        setSelectedDriverIndex(i)
                        break
                    }
                }

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
                    drivers.add(Pair(driverToSave.path, currentDriverMetadata))
                    setSelectedDriverIndex(drivers.size - 1)
                }

                _driverList.value = drivers
                _areDriversLoading.value = false
            }
        }
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
            setSelectedDriverIndex(_driverList.value.size)
            _driverList.value.add(driverData)
            _selectedDriverMetadata.value = driverData.second.name
                ?: YuzuApplication.appContext.getString(R.string.system_gpu_driver)
        } else {
            setSelectedDriverIndex(driverIndex)
        }
    }

    fun removeDriver(driverData: Pair<String, GpuDriverMetadata>) {
        _driverList.value.remove(driverData)
    }

    fun onCloseDriverManager() {
        _isDeletingDrivers.value = true
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

        if (GpuDriverHelper.customDriverData == driverList.value[selectedDriver].second) {
            return
        }

        _isDriverReady.value = false
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                if (selectedDriver == 0) {
                    GpuDriverHelper.installDefaultDriver()
                    setDriverReady()
                    return@withContext
                }

                val driverToInstall = File(driverList.value[selectedDriver].first)
                if (driverToInstall.exists()) {
                    GpuDriverHelper.installCustomDriver(driverToInstall)
                } else {
                    GpuDriverHelper.installDefaultDriver()
                }
                setDriverReady()
            }
        }
    }

    private fun setDriverReady() {
        _isDriverReady.value = true
        _selectedDriverMetadata.value = GpuDriverHelper.customDriverData.name
            ?: YuzuApplication.appContext.getString(R.string.system_gpu_driver)
    }
}
