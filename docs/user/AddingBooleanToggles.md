# User Handbook - Adding Boolean Settings Toggles

> [!WARNING]
> This guide is intended for developers ONLY. If you are not a developer, this likely irrelevant to yourself.

This guide will walk you through adding a new boolean toggle setting to Eden's configuration across both Qt's (PC) and Kotlin's (Android) UIs.

## Step 1 - src/common/settings.
Firstly add your desired toggle inside `setting.h`,
Example:
```
SwitchableSetting<bool> your_setting_name{linkage, false, "your_setting_name", Category::RendererExtensions};
```
NOTE - If you wish for your toggle to be on by default then change `false` to `true` after `linkage,`.
### Remember to add your toggle to the appropriate category, for example:
Common Categories:
- Category::Renderer
- Category::RendererAdvanced
- Category::RendererExtensions
- Category::System
- Category::Core

## Step 2 - src/qt_common/config/shared_translation.cpp
Now you can add the toggle to the QT (PC) UI inside `shared_translation.cpp`,
Find where you wish for it to appear and place it there.
Example:
```
INSERT(Settings,
       your_setting_name,
       tr("Your Setting Display Name"),
       tr("Detailed description of what this setting does.\n"
          "You can use multiple lines.\n"
          "Explain any caveats or requirements."));
```
### Make sure to:
- Keep display naming consistant
- Put detailed info in the description
- Use `\n` for line breaks in descriptions

## Step 3 - src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/model/BooleanSetting.kt
Now add it inside `BooleanSetting.kt` where it should be in the settings.
Example:
```
RENDERER_YOUR_SETTING_NAME("your_setting_name"),
```
Remember to make sure the naming of the prefix matches the desired category.

## Step 4 - src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/model/view/SettingsItem.kt
Now you may add the toggle to the Kotlin (Android) UI inside `SettingsItem.kt`.
Example:
```
put(
    SwitchSetting(
        BooleanSetting.RENDERER_YOUR_SETTING_NAME,
        titleId = R.string.your_setting_name,
        descriptionId = R.string.your_setting_name_description
    )
)
```

## Step 5 - src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/ui/SettingsFragmentPresenter.kt
Now add your setting to the correct location inside `SettingsFragmentPresenter.kt` within the right category.
Example:
```
add(BooleanSetting.RENDERER_YOUR_SETTING_NAME.key)
```
Remember, placing matters! Settings appear in the order of where you add them.

## Step 6 - src/android/app/src/main/res/values/strings.xml
Now add your setting and description to `strings.xml` in the appropriate place.
Example:
```
<string name="your_setting_name">Your Setting Display Name</string>
<string name="your_setting_name_description">Detailed description of what this setting does. Explain any caveats, requirements, or warnings here.</string>
```

## Step 7 - Use Your Toggle!
Now the UI part is done find a place in the code for the toggle,
And use it to your heart's desire!
Example:
```
const bool your_value = Settings::values.your_setting_name.GetValue();

if (your_value) {
    // Do something when enabled
}
```
If you wish to do something only when the toggle is disabled,
Use `if (!your_value) {` instead of `if (your_value) {`.


## Best Practices
- Naming - Use clear, descriptive names. Something for both the devs and the users.
- Defaults - Choose safe default values (usually false for new features).
- Documentation - Write clear descriptions explaining when and why to use the setting.
- Categories - Put settings in the appropriate category.
- Order - Place related settings near each other.
- Testing - Always test on both PC and Android before committing when possible.

### Thank you for reading, I hope this guide helped you making your toggle!