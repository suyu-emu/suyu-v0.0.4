package org.yuzu.yuzu_emu.features.settings.model

class IntSetting(
    key: String,
    section: String,
    var value: Int
) : Setting(key, section) {
    override val valueAsString = value.toString()
}
