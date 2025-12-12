# Importing Games into Steam with Steam Rom Manager

Use this when you want to import your games inside Eden into Steam to launch with artwork from Steam Game Mode without needing to launch Eden first.

**Click [Here](https://evilperson1337.notion.site/Importing-Games-into-Steam-with-Steam-Rom-Manager-2b757c2edaf680d7a491c92b138f1fcc) for a version of this guide with images & visual elements.**

---

### Pre-Requisites

- Steam Deck Set up and Configured
- Eden set up and Configured
- Internet Access

---

## Steps

1. Press the **STEAM** button and then go to *Power → Switch to Desktop* to enter the Desktop mode.

1. Install ***Steam ROM Manager***, there are 2 ways you can accomplish this, either manually or through [*EmuDeck*](https://www.emudeck.com/#downloads).

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
    
2. Open the Start Menu and Launch ***Steam ROM Manager*** 

1. The program will now launch and show you a window with parsers.
    
    <aside>
    
    ***TIP***: Your layout may look different depending on how you installed *Steam ROM Manager*.  You may need to go to **Settings → Theme** and change it to *Classic* to follow along.
    
    </aside>
    
2. Switch off all Parsers by hitting the *Toggle Parsers* switch.
3. Scroll down the list on the left-hand side and look for a parser called *Nintendo Switch - Eden* and switch it on.  This parser may not exist depending on how you installed *Steam ROM Manager* (EmuDeck creates it for you).  Follow these steps to create it if it is missing.
    
    ---
    ### Creating the Eden Parser
    
    1. Select Create Parser and in the *Community Presets* option look for **Nintendo Switch - Yuzu**.
    2. Change the **Parser title** from *Nintendo Switch - Yuzu* to *Nintendo Switch - Eden.*
    3. Hit the **Browse** option under the *ROMs directory* section.  Select the directory containing your Switch ROMs.
    4. Under *Steam collections*, you can add a Steam category name.  This just organizes the games under a common category in your Steam Library, this is optional but recommended.
    5. Scroll down slightly to the **Executable Configuration → Executable**, select **Browse** and select the Eden AppImage.
    6. Leave everything else the same and hit **Save** to save the parser.
    ---
    
4. Click the Eden parser to view the options on the right, select **Test** at the bottom of the screen to ensure that *Steam ROM Manager* detects your games correctly.
1. *Steam ROM Manager* will start to scan the specified ROMs directory and match them to games.  Look over the results to ensure they are accurate.  If you do not see any entries - check your parsers ROMs directory field.
1. When you are happy with the results, click the **Add Games** → **Parse** to start the actual Parsing.
1. The program will now identify the games and pull artwork from [*SteamGridDB*](https://www.steamgriddb.com/).
2. Review the game matches and ensure everything is there.
    
    ---

    ### Correcting a Mismatch
    
    If the game is not identified correctly, you may need to tell *Steam ROM Manager* what the game is manually.
    
    1. Hover over the game card and click the magnifying glass icon.
    2. Search for the game on the *Search SteamGridDB* section and scroll through the results, selecting the one you want.        
    3. Ensure the *Name* and *Game ID*  update in the **Per-App Exceptions** and press **Save and close**.  The game should now update.

    ---
    
    ### Excluding Matches
    
    You may want to tell Steam ROM Manager to ignore some files (updates/DLC/etc.) that it finds in the directory.  This is how you do so.
    
    1. Hit the **Exclude Games** button in the bottom right.
    2. Deselect the game you want to exclude, the poster artwork should go dim and the **Number Excluded** number should increment up.  Repeat with any other exclusions you want to add.
    3. Hit **Save Excludes** when you are happy with your selections.
    ---
3. When you are happy with the results, select **Save to Steam** to save the results. 
1. The program will now start writing the entries into the Steam Library.  You should get pop up notifications of the progress, but you can monitor the progress by selecting the **Log** on the left-hand side if needed. 
2. Restart Steam to have the changes take effect.  Check your library to ensure that your games are there, in a category if you defined one in the parser.
3. Try to launch a game and ensure everything is working.  You are now good to go.