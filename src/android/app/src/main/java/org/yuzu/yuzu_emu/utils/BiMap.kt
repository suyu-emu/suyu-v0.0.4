// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

class BiMap<K, V> {
    private val forward: MutableMap<K, V> = HashMap()
    private val backward: MutableMap<V, K> = HashMap()

    @Synchronized
    fun add(key: K, value: V) {
        forward[key] = value
        backward[value] = key
    }

    @Synchronized
    fun getForward(key: K): V? {
        return forward[key]
    }

    @Synchronized
    fun getBackward(key: V): K? {
        return backward[key]
    }
}
