// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.disk_shader_cache

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class ShaderProgressViewModel : ViewModel() {
    private val _progress = MutableLiveData(0)
    val progress: LiveData<Int> get() = _progress

    private val _max = MutableLiveData(0)
    val max: LiveData<Int> get() = _max

    private val _message = MutableLiveData("")
    val message: LiveData<String> get() = _message

    fun setProgress(progress: Int) {
        _progress.postValue(progress)
    }

    fun setMax(max: Int) {
        _max.postValue(max)
    }

    fun setMessage(msg: String) {
        _message.postValue(msg)
    }
}
