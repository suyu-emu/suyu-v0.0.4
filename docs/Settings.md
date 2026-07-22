# Settings

> [!WARNING]
> This guide is intended for developers ONLY. If you're looking for configuring the emulator itself, please read **[the user handbook](./user/README.md)**.

Settings on the emulator are very important, toggles and such can be used to guard and/or add branches to paths where some games may crash while others won't, and viceversa.

However, this process can be tedious for those unfamiliar; this document serves as a outline/documentation for the settings subsystem.

## Index

* [Adding Debug Knobs](#adding-debug-knobs)
  * [Advantages](#advantages)
  * [Usage](#usage)
    * [Accessing Debug Knobs (dev side)](#accessing-debug-knobs-dev-side)
    * [Setting Debug Knobs (user side)](#setting-debug-knobs-user-side)
    * [Bit Manipulation Examples](#bit-manipulation-examples)
  * [Terminology and user communication](#terminology-and-user-communication)
  * [Examples](#examples)
    * [Example 1: Conditional Debug Logging](#example-1-conditional-debug-logging)
    * [Example 2: Performance Tuning](#example-2-performance-tuning)
    * [Example 3: Feature Gating](#example-3-feature-gating)
  * [Best Practices](#best-practices)
* [Adding Boolean Settings Toggles](#adding-boolean-settings-toggles)
  * [Step 1 - Common Setting](#step-1-common-setting)
  * [Step 2 - Qt Toggle](#step-2-qt-toggle)
  * [Step 3 - Kotlin (Android)](#step-3-kotlin-android)
    * [Step 3.1 - BooleanSetting.kt](#step-3-1-booleansetting-kt)
    * [Step 3.2 - SettingsItem.kt](#step-3-2-settingsitem-kt)
    * [Step 3.3 - SettingsFragmentPresenter.kt](#step-3-3-settingsfragmentpresenter-kt)
    * [Step 3.4 - Localization](#step-3-4-localization)
  * [Step 4 - Use Your Toggle](#step-4-use-your-toggle)
  * [Best Practices](#best-practices)

## Adding Boolean Settings Toggles

This guide will walk you through adding a new boolean toggle setting to Eden's configuration across both Qt's (PC) and Kotlin's (Android) UIs.

---

### Step 1 - Common Setting

Firstly add your desired toggle:

Example: `src/common/setting.h`
```cpp
SwitchableSetting<bool> your_setting_name{linkage, false, "your_setting_name", Category::RendererExtensions};
```

Remember to add your toggle to the appropriate category, for example:

Common Categories:

* Category::Renderer
* Category::RendererAdvanced
* Category::RendererExtensions
* Category::System
* Category::Core

> [!WARNING]
> If you wish for your toggle to be `on by default` then change `false` to `true` after `linkage,`.

---

### Step 2 - Qt Toggle

Add the toggle to the Qt UI, where you wish for it to appear and place it there.

Example: `src/qt_common/config/shared_translation.cpp`
```cpp
INSERT(Settings,
       your_setting_name,
       tr("Your Setting Display Name"),
       tr("Detailed description of what this setting does.\n"
          "You can use multiple lines.\n"
          "Explain any caveats or requirements."));
```

#### Make sure to:

* Keep display naming consistant
* Put detailed info in the description
* Use `\n` for line breaks in descriptions

---

### Step 3 - Kotlin (Android)

#### Step 3.1 - BooleanSetting.kt

Add where it should be in the settings.

Example: `src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/model/BooleanSetting.kt`
```kts
RENDERER_YOUR_SETTING_NAME("your_setting_name"),
```

#### Make sure to:

* Ensure the prefix naming matches the intended category.

---

#### Step 3.2 - SettingsItem.kt

Add the toggle to the Kotlin (Android) UI

Example: `src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/model/view/SettingsItem.kt`
```kts
put(
    SwitchSetting(
        BooleanSetting.RENDERER_YOUR_SETTING_NAME,
        titleId = R.string.your_setting_name,
        descriptionId = R.string.your_setting_name_description
    )
)
```

---

#### Step 3.3 - SettingsFragmentPresenter.kt

Add your setting within the right category.

Example: `src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/ui/SettingsFragmentPresenter.kt`
```kts
add(BooleanSetting.RENDERER_YOUR_SETTING_NAME.key)
```

> [!WARNING]
> Remember, placing matters! Settings appear in the order of where you add them.

---

#### Step 3.4 - Localization

Add your setting and description in the appropriate place.

Example: `src/android/app/src/main/res/values/strings.xml`
```xml
<string name="your_setting_name">Your Setting Display Name</string>
<string name="your_setting_name_description">Detailed description of what this setting does. Explain any caveats, requirements, or warnings here.</string>
```

---

### Step 4 - Use Your Toggle!

Now the UI part is done find a place in the code for the toggle,
And use it to your heart's desire!

Example:
```cpp
const bool your_value = Settings::values.your_setting_name.GetValue();

if (your_value) {
    // Do something when enabled
}
```

If you wish to do something only when the toggle is disabled,
Use `if (!your_value) {` instead of `if (your_value) {`.

---

### Best Practices

* Naming - Use clear, descriptive names. Something for both the devs and the users.
* Defaults - Choose safe default values (usually false for new features).
* Documentation - Write clear descriptions explaining when and why to use the setting.
* Categories - Put settings in the appropriate category.
* Order - Place related settings near each other.
* Testing - Always test on both PC and Android before committing when possible.

Thank you for reading, I hope this guide helped you making your toggle!

## Adding Debug Knobs

Debug Knobs is a 16-bit integer setting (`debug_knobs`) in the Eden Emulator that serves as a bitmask for gating various testing and debugging features. This allows developers and advanced users to enable or disable specific debug behaviors without requiring deploying of complete but temporary toggles.

The setting ranges from 0 to 65535 (0x0000 to 0xFFFF), where each bit represents a different debug feature flag.

---

### Advantages

The main advantage is to avoid deploying new disposable toggles (those made only for testing stage, and are disposed once new feature gets good to merge). This empowers devs to be free of all frontend burocracy and hassle of new toggles.

Common advantages recap:

* **Fine-Grained Control**: Enable or disable up to 16 individual debug features independently using bit manipulation on a single build
* **Runtime Configuration**: Change debug behavior at runtime the same way as new toggles would do
* **Safe incremental development**: New debug features can be added while impact can be isolated from previous deployments

### Usage

#### Accessing Debug Knobs (dev side)

Use the `Settings::getDebugKnobAt(u8 i)` function to check if a specific bit is set:

```cpp
//cpp side
#include "common/settings.h"

// Check if bit 0 is set
bool feature_enabled = Settings::getDebugKnobAt(0);

// Check if bit 15 is set
bool another_feature = Settings::getDebugKnobAt(15);
```

```kts
//kotlin side
import org.yuzu.yuzu_emu.features.settings.model.Settings

// Check if bit x is set
bool feature_enabled = Settings.getDebugKnobAt(x); //x as integer from 0 to 15
```

The function returns `true` if the specified bit (0-15) is set in the `debug_knobs` value, `false` otherwise.

#### Setting Debug Knobs (user side)

Developers must inform which knobs are tied to each functionality to be tested.

The debug knobs value can be set through:

1. **Desktop UI**: In the Debug configuration tab, there's a spinbox for "Debug knobs" (0-65535)
2. **Android UI**: Available as an integer setting in the Debug section
3. **Configuration Files**: Set the `debug_knobs` value in the emulator's configuration

#### Bit Manipulation Examples

To enable specific features, calculate the decimal value by setting the appropriate bits:

* **Enable only bit 0**: Value = 1 (2^0)
* **Enable only bit 1**: Value = 2 (2^1)
* **Enable bits 0 and 1**: Value = 3 (2^0 + 2^1)
* **Enable bit 15**: Value = 32768 (2^15)

### Terminology and user communication

There are two main confusions when talking about knobs:

#### Whether it's zero-based or one-based

Sometimes when an user reports: knobs 1 and 2 gets better performance, dev may get confuse whether he means the knobs 1 and 2 literally, or the 1st and 2nd knobs (knobs 0 and 1).

Debug knobs are **zero-based**, which means:
* The first knob is the knob(0) (or knob0 henceforth), and the last one is the 15 (knob15, likewise)
* You can talk: "knob0 is enabled/disabled", "In this video i was using only knobs 0 and 2", etc.

#### Whether one is talking about the knob itself or about the entire parameter value (which represents all knobs)

Sometimes when an user reports: knob 3 results, it's unclear whether he's referring to knob setting with value 3 (which means both knob 0 and 1 are enabled), or to knob(3) specifically.
Whenever you're instructing tests or reporting results, be precise about whether one you're talking to avoid confusion:

#### Setting based terminology

ALWAYS use the word in PLURAL (knobs), without mentioning which one, to refer to the setting, aka multiple knobs at once:
Examples:
- **knobs=0**: no knobs enabled
- **knobs=1**: knob0 enabled, others disabled
- **knobs=2**: knob1 enabled, others disabled
- **knobs=3**: knobs 0 and 1 enabled, others disabled

...

#### Knob based terminology

Use the word in SINGULAR (knob), or in plural but referring which ones, when meaning multiple knobs at once:
Examples:
- **knob0**: knob 0 enabled, others disabled
- **knob1**: knob 1 enabled, others disabled
- **knobs 0 and 1**: knobs 0 and 1 enabled, others disabled

...

### Examples

#### Example 1: Conditional Debug Logging

```cpp
void SomeFunction() {
    if (Settings::getDebugKnobAt(0)) {
        LOG_DEBUG(Common, "Debug feature 0 is enabled");
        // Additional debug code here
    }
    
    if (Settings::getDebugKnobAt(1)) {
        LOG_DEBUG(Common, "Debug feature 1 is enabled");
        // Different debug behavior
    }
}
```

#### Example 2: Performance Tuning

```cpp
bool UseOptimizedPath() {
    // Skip optimization if debug bit 2 is set for testing
    return !Settings::getDebugKnobAt(2);
}
```

#### Example 3: Feature Gating

```cpp
void ExperimentalFeature() {
    static constexpr u8 EXPERIMENTAL_FEATURE_BIT = 3;
    
    if (!Settings::getDebugKnobAt(EXPERIMENTAL_FEATURE_BIT)) {
        // Fallback to stable implementation
        StableImplementation();
        return;
    }
    
    // Experimental implementation
    ExperimentalImplementation();
}
```

### Best Practices

* This setting is intended for development and testing purposes only
* Knobs must be unwired before PR creation
* The setting is per-game configurable, allowing different debug setups for different titles
