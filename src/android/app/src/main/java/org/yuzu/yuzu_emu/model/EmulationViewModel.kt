// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class EmulationViewModel : ViewModel() {
    private val _emulationStarted = MutableLiveData(false)
    val emulationStarted: LiveData<Boolean> get() = _emulationStarted

    private val _isEmulationStopping = MutableLiveData(false)
    val isEmulationStopping: LiveData<Boolean> get() = _isEmulationStopping

    private val _shaderProgress = MutableLiveData(0)
    val shaderProgress: LiveData<Int> get() = _shaderProgress

    private val _totalShaders = MutableLiveData(0)
    val totalShaders: LiveData<Int> get() = _totalShaders

    private val _shaderMessage = MutableLiveData("")
    val shaderMessage: LiveData<String> get() = _shaderMessage

    fun setEmulationStarted(started: Boolean) {
        _emulationStarted.postValue(started)
    }

    fun setIsEmulationStopping(value: Boolean) {
        _isEmulationStopping.value = value
    }

    fun setShaderProgress(progress: Int) {
        _shaderProgress.value = progress
    }

    fun setTotalShaders(max: Int) {
        _totalShaders.value = max
    }

    fun setShaderMessage(msg: String) {
        _shaderMessage.value = msg
    }

    fun updateProgress(msg: String, progress: Int, max: Int) {
        setShaderMessage(msg)
        setShaderProgress(progress)
        setTotalShaders(max)
    }

    fun clear() {
        _emulationStarted.value = false
        _isEmulationStopping.value = false
        _shaderProgress.value = 0
        _totalShaders.value = 0
        _shaderMessage.value = ""
    }
}
