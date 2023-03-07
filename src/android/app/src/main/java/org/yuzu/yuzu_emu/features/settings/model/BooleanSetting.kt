package org.yuzu.yuzu_emu.features.settings.model

class BooleanSetting(
    key: String,
    section: String,
    var value: Boolean
) : Setting(key, section) {
    override val valueAsString = if (value) "True" else "False"
}
