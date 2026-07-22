# User Handbook - Installing Mods

## General Notes

**Note:** When installing a mod, always read the mod's installation instructions.

This is especially important if a mod uses a framework such as **ARCropolis**, **Skyline**, or **Atmosphere plugins**. In those cases, follow the framework's instructions instead of using Eden's normal mod folder.

For example, **Super Smash Bros. Ultimate** uses such a framework. See the related section below for details.

---

# Installing Mods for Most Games

1. Right click a game in the game list.
2. Click **"Open Mod Data Location"**.
3. Extract the mod into that folder.

Each mod should be placed inside **its own subfolder**.

---

# Enabling or Disabling Mods

1. Right click the game in the game list.
2. Click **Configure Game**.
3. In the **Add-Ons** tab, enable or disable mods, updates, and DLC by ticking or unticking their boxes.

---

# Important Note About SD Card Paths

Some mods are designed for real Nintendo Switch consoles and refer to the **SD card root**.

The emulated SD card is located at:

```
%AppData%\eden\sdmc
```

Example:

```
Switch instruction:  sd:/ultimate/mods
Eden equivalent:     sdmc/ultimate/mods
```

---

# Framework-Based Mods (Super Smash Bros. Ultimate)

Some games require external mod frameworks instead of the built-in mod loader.

The most common example is **Super Smash Bros. Ultimate**.

These mods are installed directly to the **emulated SD card**, not the normal Eden mod folder.

---

# Installing the ARCropolis Modding Framework

**Note:** Some mod packs bundle ARCropolis with their installer (for example, Smash Ult-S).

---

## 1. Download ARCropolis

Download the latest release:

https://github.com/Raytwo/ARCropolis/releases/

---

## 2. Install ARCropolis

Extract the **`atmosphere`** folder into:

```
%AppData%\eden\sdmc
```

This is the **emulated SD card directory**.

Verify installation by checking that the following file exists:

```
sdmc\atmosphere\contents\01006A800016E000\romfs\skyline\plugins\libarcropolis.nro
```

---

## 3. Download Skyline

Download the latest Skyline release:

https://github.com/skyline-dev/skyline/releases

Skyline used to be bundled with ARCropolis but is now distributed separately to avoid compatibility issues caused by outdated bundled versions.

---

## 4. Install Skyline

Extract the **`exefs`** folder into:

```
sdmc\atmosphere\contents\01006A800016E000
```

The `exefs` folder should be **next to the `romfs` folder**.

Verify installation by checking that the following file exists:

```
%AppData%\eden\sdmc\atmosphere\contents\01006A800016E000\exefs\subsdk9
```

---

## 5. Launch the Game Once

Start the game and make sure you see the **ARCropolis version text on the title screen**.

This will also create the folders required for installing mods.

---

## 6. Install Smash Ultimate Mods

Install mods inside:

```
sdmc\ultimate\mods
```

Each mod must be placed inside **its own subfolder**.

Example:

```
sdmc\ultimate\mods\ExampleMod
```

---

# Troubleshooting

## ARCropolis text does not appear on startup

Check the following:

- `libarcropolis.nro` exists in:

```
sdmc\atmosphere\contents\01006A800016E000\romfs\skyline\plugins
```

- `subsdk9` exists in:

```
sdmc\atmosphere\contents\01006A800016E000\exefs
```

- Files were extracted to:

```
%AppData%\eden\sdmc
```

---

## Mods are not loading

Make sure mods are installed inside:

```
sdmc\ultimate\mods
```

Each mod must have its **own subfolder**.

Correct example:

```
sdmc\ultimate\mods\ExampleMod
```

Incorrect example:

```
sdmc\ultimate\mods\ExampleMod\ExampleMod
```

---

## Installing mods in the wrong folder

ARCropolis mods **do not go in Eden's normal mod folder**.

Do **not** install Smash mods here:

```
user\load\01006A800016E000
```

That folder is only used for traditional **RomFS mods**, not ARCropolis.
