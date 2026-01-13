# User Handbook - Adding Boolean Settings Toggles

> [!WARNING]
> This guide is intended for developers ONLY. If you are not a developer, this likely irrelevant to yourself.
>
> If you want to add temporary toggles, please refer to **[Adding Debug Knobs](AddingDebugKnobs.md)**

This guide will walk you through adding a new boolean toggle setting to Eden's configuration across both Qt's (PC) and Kotlin's (Android) UIs.

## Index

1. [Step 1 - Common Setting](#step-1-common-setting)
2. [Step 2 - Qt Toggle](#step-2-qt-toggle)
3. [Step 3 - Kotlin (Android)](#step-3-kotlin-android)

   * [Step 3.1 - BooleanSetting.kt](#step-3-1-booleansetting-kt)
   * [Step 3.2 - SettingsItem.kt](#step-3-2-settingsitem-kt)
   * [Step 3.3 - SettingsFragmentPresenter.kt](#step-3-3-settingsfragmentpresenter-kt)
   * [Step 3.4 - Localization](#step-3-4-localization)
4. [Step 4 - Use Your Toggle](#step-4-use-your-toggle)
5. [Best Practices](#best-practices)

---

## Step 1 - Common Setting

Firstly add your desired toggle:

Example: `src/common/setting.h`
```cpp
SwitchableSetting<bool> your_setting_name{linkage, false, "your_setting_name", Category::RendererExtensions};
```

### Remember to add your toggle to the appropriate category, for example:

Common Categories:

* Category::Renderer
* Category::RendererAdvanced
* Category::RendererExtensions
* Category::System
* Category::Core

> [!WARNING]
> If you wish for your toggle to be `on by default` then change `false` to `true` after `linkage,`.

---

## Step 2 - Qt Toggle

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

### Make sure to:

* Keep display naming consistant
* Put detailed info in the description
* Use `\n` for line breaks in descriptions

---

## Step 3 - Kotlin (Android)

### Step 3.1 - BooleanSetting.kt

Add where it should be in the settings.

Example: `src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/model/BooleanSetting.kt`
```kts
RENDERER_YOUR_SETTING_NAME("your_setting_name"),
```

### Make sure to:

* Ensure the prefix naming matches the intended category.

---

### Step 3.2 - SettingsItem.kt

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

### Step 3.3 - SettingsFragmentPresenter.kt

Add your setting within the right category.

Example: `src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/ui/SettingsFragmentPresenter.kt`
```kts
add(BooleanSetting.RENDERER_YOUR_SETTING_NAME.key)
```

> [!WARNING]
> Remember, placing matters! Settings appear in the order of where you add them.

---

### Step 3.4 - Localization

Add your setting and description in the appropriate place.

Example: `src/android/app/src/main/res/values/strings.xml`
```xml
<string name="your_setting_name">Your Setting Display Name</string>
<string name="your_setting_name_description">Detailed description of what this setting does. Explain any caveats, requirements, or warnings here.</string>
```

---

## Step 4 - Use Your Toggle!

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

## Best Practices

* Naming - Use clear, descriptive names. Something for both the devs and the users.
* Defaults - Choose safe default values (usually false for new features).
* Documentation - Write clear descriptions explaining when and why to use the setting.
* Categories - Put settings in the appropriate category.
* Order - Place related settings near each other.
* Testing - Always test on both PC and Android before committing when possible.

### Thank you for reading, I hope this guide helped you making your toggle!
