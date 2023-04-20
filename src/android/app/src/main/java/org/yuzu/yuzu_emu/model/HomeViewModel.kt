package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class HomeViewModel : ViewModel() {
    private val _navigationVisible = MutableLiveData(true)
    val navigationVisible: LiveData<Boolean> get() = _navigationVisible

    private val _statusBarShadeVisible = MutableLiveData(true)
    val statusBarShadeVisible: LiveData<Boolean> get() = _statusBarShadeVisible

    var navigatedToSetup = false

    fun setNavigationVisibility(visible: Boolean) {
        if (_navigationVisible.value == visible) {
            return
        }
        _navigationVisible.value = visible
    }

    fun setStatusBarShadeVisibility(visible: Boolean) {
        if (_statusBarShadeVisible.value == visible) {
            return
        }
        _statusBarShadeVisible.value = visible
    }
}
