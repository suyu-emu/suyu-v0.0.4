// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class TaskViewModel : ViewModel() {
    private val _result = MutableLiveData<Any>()
    val result: LiveData<Any> = _result

    private val _isComplete = MutableLiveData<Boolean>()
    val isComplete: LiveData<Boolean> = _isComplete

    private val _isRunning = MutableLiveData<Boolean>()
    val isRunning: LiveData<Boolean> = _isRunning

    lateinit var task: () -> Any

    init {
        clear()
    }

    fun clear() {
        _result.value = Any()
        _isComplete.value = false
        _isRunning.value = false
    }

    fun runTask() {
        if (_isRunning.value == true) {
            return
        }
        _isRunning.value = true

        viewModelScope.launch(Dispatchers.IO) {
            val res = task()
            _result.postValue(res)
            _isComplete.postValue(true)
        }
    }
}
