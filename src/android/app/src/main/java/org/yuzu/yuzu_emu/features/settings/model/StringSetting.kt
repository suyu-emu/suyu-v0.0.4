package org.yuzu.yuzu_emu.features.settings.model

class StringSetting(
    key: String,
    section: String,
    var value: String
) : Setting(key, section) {
    override var valueAsString = value
}
