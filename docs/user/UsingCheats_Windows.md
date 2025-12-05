# Using Cheats with Eden

Use this guide when you want to add cheats into a game to alter gameplay for use with the Eden emulator.

**Click [Here](https://evilperson1337.notion.site/Using-Cheats-with-Eden-2b057c2edaf6818fab66c276e2304bb4) for a version of this guide with images & visual elements.**

---

### Pre-Requisites

- Eden Emulator fully set up and configured
- The cheat(s) you want to apply
- The **Build ID** of the game.

<aside>

***TIP***: The easiest way I have found to find the Build ID is by Right-Clicking the game **IN RYUJINX** and hitting **Manage Cheats**.  Your Build ID will be displayed at the top. 

Another option would be to launch the game in Eden and close it - then go into the log and look for a line like this - the first 16 characters if your Build ID. **Make sure that it is the MAIN line**.
`[  27.098382] Loader <Info> core/file_sys/patch_manager.cpp:HasNSOPatch:304: Querying NSO patch existence for build_id=AEE6DCCC06D9C05B42061E2019123A61, name=main`

</aside>

## Steps

---

### Configuring a Cheat

1. Copy the Hex Code of the cheat into a text file, optionally with the cheat name at the beginning like the example.  Here this code will set the timer to 999 in *New Super Mario Bros. U Deluxe.*

    ```bash
    [Time = 999]
    58000000 00C88A70
    78001000 00000090
    64000000 00000000 003E6F00
    ```

1. Save the file as a **txt** file with the Build ID of the game.  For my example, my Build ID is **AEE6DCCC06D9C05B** so my file would be `AEE6DCCC06D9C05B.txt`.
2. Launch Eden and wait for the program to load.
3. *Right-Click* the game in Eden and select **Open Mod Data Location**.  A file explorer window should appear.
4. Create a folder inside of the file explorer window with the name of the cheat.  This name does not matter and only affects how it appears in the game properties inside of Eden.
5. Navigate inside of this folder and create another folder called **cheats.**
6. Move the txt file you created earlier into this **cheats** folder. (e.g. `<mod_location>/Time 999/cheats/AEE6DCCC06D9C05B.txt` )
7. Go back to Eden and *right-click* the game.  Select *Configure Game* and you should now see the cheat you created appear in the **Add-Ons** section with the name of the folder from step 6.
8. Launch the game to verify that the cheat is enabled.

### Multiple Cheats

In order to install multiple cheats, you must repeat the steps above with the new cheat, creating a new directory with the name of the cheat and cheats directory.  You **cannot** install multiple cheats with a single file.

Community Member [Ninjistix](https://github.com/Ninjistix) created a utility (Windows or anything that can run Python) that can take a file with multiple cheats and create the files/structure for you with a provided Build ID.  To download and run it, see the [GitHub Project](https://github.com/Ninjistix/nxCheat_Splitter) page.

**Example cheat TXT file with multiple cheats.  It must be in this format to work:**
```
[Super Mario Bros. Wonder - Various] <- Optional

[♯ 1. Always Star Power]
040E0000 00880580 52800035

[♯ 2. Star Power + Bubble Mode (Invincible)]
040E0000 00880580 52800075

[♯ 3. Can Fast Travel to Any Course and World]
040E0000 00935E10 52800036
040E0000 0048A528 52800028
040E0000 005D9F58 52800028

[♯ 4. Got All Top of Flag Poles]
040E0000 0048A818 52800028
```

---

### Enabling/Disabling Cheats

Cheats are enabled by default, but can be disabled so they don’t affect gameplay fairly easily using the game properties.

1. *Right-Click* the game and select *Configure Game*.
2. In the **Add-Ons** section, locate the cheat you wish to enable.
3. *Select/Deselect* the name of the cheat you wish to enable/disable.
4. Click **OK** to close the window.
5. Launch the game to confirm the cheat is/is not active.