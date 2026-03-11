# User Handbook - Third party tools and extras

The Eden emulator by itself lacks some functionality - or otherwise requires external files (such as packaging) to operate correctly in a given OS. Addendum to that some repositories provide nightly or specialised builds of the emulator.

While most of the links mentioned in this guide are relatively "safe"; we urge users to use their due diligence and appropriatedly verify the integrity of all files downloaded and ensure they're not compromised.

- [NixOS Eden Flake](https://github.com/Grantimatter/eden-flake)
- [ES-DE Frontend Support](https://github.com/GlazedBelmont/es-de-android-custom-systems)

## Mirrors

The main origin repository is always at https://git.eden-emu.dev/eden-emu/eden.

- https://github.com/eden-emulator/mirror
- https://git.crueter.xyz/mirror/eden
- https://collective.taymaerz.de/eden/eden

Other mirrors obviously exist on the internet, but we can't guarantee their reliability and/or availability.

If you're someone wanting to make a mirror, simply setup forgejo and automatically mirror from the origin repository. Or you could mirror a mirror to save us bandwidth... your choice!

## Configuring Obtainium

Very nice handy app, here's a quick rundown how to configure:

1. Copy the URL: https://git.eden-emu.dev/eden-emu/eden/ (or one of your favourite mirrors)
2. Open Obtainium and tap `Add App`.
3. Paste the URL into the `App Source URL` field.
4. Override Source: Look for the `Override Source` dropdown menu and select `Forgejo (Codeberg)`.
5. Click `Add:` Obtainium should now be able to parse the releases and find the APK files.

Note: Even though the site isn't Codeberg, it uses the same Forgejo/Gitea backend, and this setting tells Obtainium how to read the release data.

## Configuring ES-DE

### Method 1

1. Download ZIP from [here](https://github.com/GlazedBelmont/es-de-android-custom-systems)
2. Unzip the file and extract `es_systems.xml` and `es_find_rules.xml` to `\Odin2\Internal shared storage\ES-DE\custom_systems`.
3. Press `Start -> Other Settings -> Alternative Emulators` and set it to Eden (Standalone).

### Method 2

1. Navigate to `\Odin2\Internal shared storage\ES-DE\custom_systems`.
2. Add this to your `es_find_rules.xml`:

```xml
<!-- Standard aka. normal release -->
<emulator name="EDEN">
    <rule type="androidpackage">
        <entry>dev.eden.eden_emulator/org.yuzu.yuzu_emu.activities.EmulationActivity</entry>
    </rule>
</emulator>

<!-- Optimized -->
<emulator name="EDEN">
    <rule type="androidpackage">
        <entry>com.miHoYo.Yuanshen/org.yuzu.yuzu_emu.activities.EmulationActivity</entry>
    </rule>
</emulator>
```

3. Add this line of text to your `es_systems.xml` underneath where the rest of your switch system entries are:

```xml
<command label="Eden (Standalone)">%EMULATOR_EDEN% %ACTION%=android.nfc.action.TECH_DISCOVERED %DATA%=%ROMPROVIDER%</command>
```

## Configuring GameMode

There is a checkbox to enable gamemode automatically. The `libgamemode.so` library must be findable on the standard `LD_LIBRARY_PATH` otherwise it will not properly be enabled. If for whatever reason it doesn't work, see [Arch wiki: GameMode](https://wiki.archlinux.org/title/GameMode) for more info.

You may launch the emulator directly via the wrapper `gamemode <program>`, and things should work out of the box.
