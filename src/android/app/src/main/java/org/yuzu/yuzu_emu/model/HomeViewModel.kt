package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class HomeViewModel : ViewModel() {
    private val _navigationVisible = MutableLiveData(true)
    val navigationVisible: LiveData<Boolean> get() = _navigationVisible

    fun setNavigationVisible(visible: Boolean) {
        if (_navigationVisible.value == visible) {
            return
        }
        _navigationVisible.value = visible
    }
}
