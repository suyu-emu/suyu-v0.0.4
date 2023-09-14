// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class TaskViewModel : ViewModel() {
    val result: StateFlow<Any> get() = _result
    private val _result = MutableStateFlow(Any())

    val isComplete: StateFlow<Boolean> get() = _isComplete
    private val _isComplete = MutableStateFlow(false)

    val isRunning: StateFlow<Boolean> get() = _isRunning
    private val _isRunning = MutableStateFlow(false)

    lateinit var task: () -> Any

    fun clear() {
        _result.value = Any()
        _isComplete.value = false
        _isRunning.value = false
    }

    fun runTask() {
        if (isRunning.value) {
            return
        }
        _isRunning.value = true

        viewModelScope.launch(Dispatchers.IO) {
            val res = task()
            _result.value = res
            _isComplete.value = true
            _isRunning.value = false
        }
    }
}
