# Importing Eden into Steam with Steam Rom Manager

Use this when you want to import the Eden AppImage into your Steam Library along with artwork using *Steam ROM Manager.*

**Click [Here](https://evilperson1337.notion.site/Importing-Eden-into-Steam-with-Steam-Rom-Manager-2b757c2edaf68054851bc287b6382cb5) for a version of this guide with images & visual elements.**

---

### Pre-Requisites

- Eden set up and configured
- Internet Connection
- Comfort Accessing and Navigating SteamOS Desktop Mode

---

## Steps

### Initial Setup

1. Press the **STEAM** button and then go to *Power → Switch to Desktop* to enter the Desktop mode.

2. Install ***Steam ROM Manager*** (if needed), there are 2 ways you can accomplish this, either manually or through [*EmuDeck*](https://www.emudeck.com/#downloads).

    ---

    ### Manual Installation

    1. Open the *Discover Store* and search for *Steam ROM Manager.*
    2. Select the **Install** button to install the program.

    ---

    ### Installing Through *EmuDeck*

    <aside>

    ***NOTE***: This assumes you have already set up EmuDeck, if not - just run through the guided installation and select *Steam ROM Manager* as one of the options.

    </aside>

    1. Open **EmuDeck**, then navigate to *Manage Emulators.*
    2. Scroll down to the bottom of the page to the *Manage your Tools & Frontends* section. Click **Steam ROM Manager**.
    3. Click the **Install** button on the right hand side to install it.

    ---

### Adding Eden into *Steam ROM Manager*

### EmuDeck Users

EmuDeck will automatically create an *Emulators - Emulators* parser for ***Steam ROM Manager*** that uses shell scripts to launch them.  We will follow this convention.

1. In the file explorer go to your **EmuDeck installation folder → tools → launchers**
2. Right-Click some empty space and hit **Create New → Text File,** call this new file ***eden.sh*** instead of ***Text File.txt***
3. Right-Click the ***eden.sh*** file you created and hit ***Open with Kate***.
4. Paste the following code into the contents of the file, save and close the file.
    
    ```bash
    #!/bin/bash
    emuName="eden" #parameterize me
    
    . "$HOME/.config/EmuDeck/backend/functions/all.sh"
    emulatorInit "$emuName"
    
    # find full path to emulator appimage
    appimage=$(find "$emusFolder" -iname "${emuName}*.AppImage" -print -quit 2>/dev/null)
    
    # make sure the appimage is executable
    	chmod +x "$appimage"
    	set -- "$appimage" "$@"
    
    echo "Launching ${emuName} with:" "$@"
    "$@"
    
    cloud_sync_uploadForced
    rm -rf "$savesPath/.gaming"
    ```
    
5. Open a terminal in the directory containing the ***eden.sh*** file and run the following command to make it executable.
    
    ```bash
    chmod u+x ./eden.sh
    ```
    
6. Proceed to the Adding the Emulator section

---

### Non-EmuDeck Users

We will need to create a new parser for the Emulators.  Unlike with the EmuDeck model, we will have the parser look for AppImages.

<aside>

***TIP***: In order to ensure that the matches occur correctly, it is recommended that you name the Eden Appimage as ***eden.AppImage***, rather than what it downloads as.

</aside>

1. Open *Steam ROM Manager* and choose **Create Parser**.
    
    <aside>
        
    ***TIP***: You may need to go to **Settings → Theme** and set it to *Classic* to view this option.
    
    </aside>

2. Add the following settings to create the parser.
    
    1. Basic Configuration
        1. **Parser Type**: *Blob*
        2. **Parser Title**: *Emulators - Emulators*
        3. **Steam Directory**: *${steamdirglobal}*
        4. **User Accounts**: *Global*
        5. **ROMs Directory**: <path to directory containing eden AppImage>
        6. **Steam Collections**: *Emulation* (OPTIONAL)
    2. Parser Specific Configuration
        1. **Search Glob**: *${title}@(.AppImage|.APPIMAGE|.appimage)*
    3. Executable Configuration
        1. **Executable Modifier**: *"${exePath}”*
    4. Title Modification Configuration
        1. **Title Modifier**: *${fuzzyTitle}*
    
3. Hit the **Test** button to ensure your emulator AppImages. 
4. Hit **Save** to save the Parser.

---

### Adding Eden to Steam

Now that we have the parser or shell script created, we can actually add it to Steam.

1. Open *Steam ROM Manager* if it is not already open.
2. Toggle the **Emulators - Emulators** parser on and hit ***Add Games*** in the top left.    
3. Click **Parse** to identify the emulators.
4. Make sure all your emulators are showing up and have the right matches.
    
    ---

    ### Correcting a Mismatch
    
    If the emulator is not identified correctly, you may need to tell *Steam ROM Manager* what the game is manually.
    
    1. Hover over the emulator card and click the magnifying glass icon.  Here it incorrectly identified *Eden* as a game by a similar name. **        
    2. Search for *Eden Emulator* on the *Search SteamGridDB* section and scroll through the results, selecting the one you want.        
    3. Ensure the *Name* and *Game ID*  update in the **Per-App Exceptions** and press **Save and close**.  The game should now update.

    ---
    
    ### Excluding Matches
    
    You may want to tell Steam ROM Manager to ignore some files that it finds in the directory.  This is how you do so.
    
    1. Hit the **Exclude Games** button in the bottom right.        
    2. Deselect the game you want to exclude, the poster artwork should go dim and the **Number Excluded** number should increment up.  Repeat with any other exclusions you want to add.        
    3. Hit **Save Excludes** when you are happy with your selections.
    
    ---

5. The program will now start writing the entries into the Steam Library.  You should get pop up notifications of the progress, but you can monitor the progress by selecting the **Log** on the left-hand side if needed. 
6. Restart Steam to have the changes take effect.  Check your library to ensure that your games are there, in a category if you defined one in the parser.    
7. Try to launch the Emulator from Steam and ensure everything is working.  You are now good to go.