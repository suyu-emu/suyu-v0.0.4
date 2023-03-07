package org.yuzu.yuzu_emu.features.settings.model

/**
 * Abstraction for a setting item as read from / written to yuzu's configuration ini files.
 * These files generally consist of a key/value pair, though the type of value is ambiguous and
 * must be inferred at read-time. The type of value determines which child of this class is used
 * to represent the Setting.
 */
abstract class Setting(
    /**
     * @return The identifier used to write this setting to the ini file.
     */
    val key: String,
    /**
     * @return The name of the header under which this Setting should be written in the ini file.
     */
    val section: String
) {

    /**
     * @return A representation of this Setting's backing value converted to a String (e.g. for serialization).
     */
    abstract val valueAsString: String
}
