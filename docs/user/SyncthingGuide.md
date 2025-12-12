# User Handbook - Backing Up/Syncing Eden Game Saves

Use this guide for when you want to configure automated backup/syncing of your Eden save files using [*Syncthing*](https://syncthing.net/).

**Click [Here](https://evilperson1337.notion.site/Backing-Up-Syncing-Eden-Game-Saves-2b357c2edaf68000b40cfab2c2c3dc0a) for a version of this guide with images & visual elements.**

### Pre-Requisites

- Eden already installed, configured, and functioning.
- Devices to run Syncthing on.
- Ability to allow a program to communicate through the firewall of your device.

## Introduction

<aside>

***WARNING***: You should manually back up your save files before proceeding with this guide.  If you incorrectly perform the steps, you risk losing them!

</aside>

- While this is a de-centralized model without the concepts of a Server/Client, Parent/Child, etc. - For the purposes of these guides, we will borrow from this models terminology to avoid sync conflicts and potential data loss.  After the initial setup, all the devices in the sync network are equals and can push & pull files from any other device.
- In order for this to work, you should get all of the save files in Eden in the save folder on the Parent.
    - If you need help doing that, see the ***Importing Saves into Eden*** guide for the platform you elect to act as the Parent, and delete the save files on the "Child" devices.

### Terminology

- **Sync Network**: All the devices configured in *Syncthing* to push/pull files.
- **Parent**: This will be the device that you elect to push files to the other devices.  There can only be one here initially in order to avoid sync conflicts.
- **Child**: All the other devices added to the Sync Network.  These devices will pull files from the Parent.

## Overview

Rather than giving a breakdown of all the platforms and configurations, those will be in the platform’s specific guides - this will serve as a general overview of Syncthing.

### What is Syncthing Anyway?

Syncthing is a continuous file synchronization program (in the layman’s - make sure 2 or more systems with the same files are always up to date).  This is perfect for game saves where we would want to play on 1 device, save our game, and then continue playing it on another device.  This technology is what Epic/Steam/etc. use to allow you to do this on games run through their respective services.  Syncthing is an open source implementation of this technology that you control, rather than relying on a 3rd party.  This has a few key benefits, most notably - better security, privacy, and speed (when on your LAN).

### What are some common issues?

Syncthing is fairly robust and doesn’t have many issues luckily, but there are some things you should watch out for (almost all of them a user issue).

- Sync conflicts
    - If for whatever reason you update the same file on 2 different machines, the system does not know which updated file is considered the one to sync across.  This results in a ***sync conflict*** where it may not sync the files as you would expect.  Worst case scenario, this can result in your save progress being lost if you are not careful.  When one of these occurs, it will create a copy of the file and store it with a specific name, like this example, *Paper Mario.sync-conflict-20251102-072925-TZBBN6S.srm.* To resolve this, you must remove the other files and remove the *.sync-conflict-<TIMESTAMP>-<Syncthing Device ID>* from the file name of the file you want to keep.
- Accidental Deletions
    - If you delete a file from one of the devices, it will also remove the file on the other devices when they perform a sync so be careful when doing this.

## Windows

### Pre-Requisites

- Eden already installed, configured, and functioning.
- Ability to allow a program to communicate through the firewall in Windows.
- Ability to extract archive (.zip/.7z/.rar) files.

### Steps

<aside>

***WARNING***: You should manually back up your save files before proceeding with this guide.  If you incorrectly perform the steps, you risk losing them!

</aside>

#### Downloading and Installing *Syncthing*

1. Download [*Syncthing Tray*](https://martchus.github.io/syncthingtray/#downloads-section).
    1. While it is available as a command line interface, for most people I would recommend *Syncthing Tray* on Windows.  For most people here, you would download the **64-bit (Intel/AMD)** version.
2. Open the downloaded archive and extract the **syncthingtray.exe** to wherever you want to store the executable.
3. Double-Click the application to run it, select the **Start guided setup** on the splash screen that appears and press **Next**.
    
    <aside>
    
    ***NOTE***: You may get a Windows Defender Smart Screen pop up, this is a known thing, just accept and run anyway.
    
    </aside>
4. It will then look for an existing Syncthing instance to pull settings from, but will likely fail to do so if you are here.  Regardless, select the **Yes, continue configuration** option.
5. Select ***Start Syncthing application that is built into Syncthing Tray***, this means it will use a built in Syncthing executable rather than relying on an externally provided one. Press **Next** to continue. 
6. Check the box to start Syncthing Tray on login - as the name implies, this means the program will run automatically whenever you log onto the computer.  Press Next to continue.
7. You will now be presented with a confirmation window with your selections, confirm they are what you want and hit **Apply** to continue.
8. You will now be prompted with a confirmation window and a message to allow it through the firewall.  Allow the access through the firewall to close that pop up.  The confirmation screen has a QR code and the devices identifier - you will need one of these to add other devices to the sync system.
9. *Syncthing/Syncthing Tray* are now installed.

#### Configuring this Machine as a Parent

Use this when you want to set this machine as the initial source of truth (push files out to all the other devices).  Afterwards they will all be equal partners, not a parent/child relationship, this just helps with initial setup.

1. Right-Click the *Syncthing* Tray icon in your taskbar and select **Open Syncthing.**
2. You will now have a browser window open up to a web GUI to configure *Syncthing*.  You will get a pop up about allowing anonymous usage and setting a password, make your selections to close them.
3. We’ll start by adding the folder with our save files that we want to sync by Pressing **+ Add Folder**.
4. A pop-up window will appear, fill in the Folder label field with whatever you want to call it, like Switch Saves.
5. Enter the Full folder path to where your save files are stored on this machine.
    
    <aside>
    
    ***TIP***: The easiest way to do this would be to open Eden, right-click a game that has a save, hit ***Open Save Data Location,*** and then go up 1 directory.  It should contain folders with the TitleID of your games.
    
    It should look similar to this: ..*\nand\user\save\0000000000000000\EC573727F509799675F6E5112C581D7E*
    
    </aside>
    
6. Ignore the other tabs for now and hit **Save**.
7. The folder is now ready to be shared with other devices.

#### Configuring this Machine as a Child

Use this when you want to set this machine up as a child (pull files from the other devices).  Afterwards they will all be equal partners, not a parent/child relationship, this just helps with initial setup.

1. Install Syncthing Tray on the client device following the section above.  Copy the child’s ID and store it so it is accessible to the Parent.
2. ***ON THE PARENT***: Right-Click the *Syncthing* Tray icon in your taskbar and select **Open Syncthing** if it is not open already**.**
3. You will now have a browser window open up to a web GUI to configure *Syncthing*.  You will get a pop up about allowing anonymous usage and setting a password, make your selections to close them.
4. Navigate down to **+ Add Remote Device**, we are going to add our Child device, so I hope you have its ID handy.  If not, go back and get it.
5. Add the ID and Name the device, the device may appear as a **nearby device**, in which case you can just click it to pre-populate the Device ID.
6. Click the **Sharing** Tab, and check the box next to the folder you set up on the Parent (Switch Saves in my case). Hit **Save.**
7. We are done with the parent, now **SWITCH OVER TO THE CHILD.**
8. ***ON THE CHILD***:  Right-Click the *Syncthing* Tray icon in your taskbar and select **Open Syncthing** if it is not open already**.** 
9. You should now see a connection request from the parent.  Hit **+ Add Device** to add the device.
10. Hit **Save** to finish adding the device.
11. That pop-up will close and you will get notification that the device wants to share a folder now. Hit **Add.**
12. Enter the path to the save folder in Eden and hit **Save.**
    
    <aside>
    
    ***TIP***: The easiest way to do this would be to open Eden, right-click a game that has a save, hit ***Open Save Data Location,*** and then go up 1 directory.  It should contain folders with the TitleID of your games.
    
    It should look similar to this: ..*\nand\user\save\0000000000000000\EC573727F509799675F6E5112C581D7E*
    
    </aside>

13. *Syncthing* will now pull all the files from the Parent and store them in your local save directory.  At this point the files are in sync and alterations to one will affect the other and both can be considered “*Parents*” for other devices you want to add.  Repeat these steps for as many devices you want.

## Linux

### Pre-Requisites

- Eden already installed, configured, and functioning.

### Step 1: Downloading and Installing Syncthing

<aside>

***WARNING***: You should manually back up your save files before proceeding with this guide.  If you incorrectly perform the steps, you risk losing them!

</aside>

<aside>

***NOTE***: I am using Linux Mint for my guides, but the steps should translate pretty easily to your distro.  I ***hope*** that if you are running Linux you know the basic operations.  Steam Deck users should follow the guide specific to that platform.

</aside>

1. Download [*Syncthing Tray*](https://flathub.org/en/apps/io.github.martchus.syncthingtray) from the Flatpak store.
2. Launch *Syncthing Tray* to run it, select the **Start guided setup** on the splash screen that appears and press **Next**.
3. It will then look for an existing *Syncthing* instance to pull settings from, but will likely fail to do so if you are here.  Regardless, select the **Yes, continue configuration** option.
4. Select ***Start installed Syncthing application via Syncthing Tray***, this means it will use a built in Syncthing executable rather than relying on an externally provided one. Press **Next** to continue.
5. You will now be presented with a confirmation window with your selections, confirm they are what you want and hit **Apply** to continue.
6. You will now be prompted with a confirmation window that has a QR code and the devices identifier - you will need one of these to add other devices to the sync system.
7. *Syncthing/Syncthing Tray* are now installed.  Press Finish to close the pop up.
    <aside>

    ***NOTE***: By default due to flatpak sandboxing limitations, Syncthing Tray will not run automatically on login.  You can get around this by following the [*instructions here*](https://github.com/flathub/io.github.martchus.syncthingtray).

    </aside>

### Step 2: Configuring this Machine as a Parent

Use this when you want to set this machine as the initial source of truth (push files out to all the other devices).  Afterwards they will all be equal partners, not a parent/child relationship, this just helps with initial setup.

1. Right-Click the *Syncthing* Tray icon in your taskbar and select **Open Syncthing.** 
    1. If you don’t have a taskbar in your distro, you can also reach it directly by opening a web browser to: *http://127.0.0.1:8384/.*    
2. You will now have a browser window open up to a web GUI to configure *Syncthing*.  You will get a pop up about allowing anonymous usage and setting a password, make your selections to close them.
3. We’ll start by adding the folder with our save files that we want to sync by Pressing **+ Add Folder**.
4. A pop-up window will appear, fill in the Folder label field with whatever you want to call it, like Switch Saves.
5. Enter the Full folder path to where your save files are stored on this machine.
    
    <aside>
    
    ***TIP***: The easiest way to do this would be to open Eden, right-click a game that has a save, hit ***Open Save Data Location,*** and then go up 1 directory.  It should contain folders with the TitleID of your games.
    
    It should look similar to this: ..*\nand\user\save\0000000000000000\EC573727F509799675F6E5112C581D7E*
    
    </aside>
    
6. Ignore the other tabs for now and hit **Save**.
7. The folder is now ready to be shared with other devices.

### Step 3: Configuring this Machine as a Child

Use this when you want to set this machine up as a child (pull files from the other devices).  Afterwards they will all be equal partners, not a parent/child relationship, this just helps with initial setup.

1. Install Syncthing Tray on the client device following the section above.  Copy the child’s ID and store it so it is accessible to the Parent.
2. ***ON THE PARENT***: Right-Click the *Syncthing* Tray icon in your taskbar and select **Open Syncthing** if it is not open already**.** 
3. You will now have a browser window open up to a web GUI to configure *Syncthing*.  You will get a pop up about allowing anonymous usage and setting a password, make your selections to close them.
4. Navigate down to **+ Add Remote Device**, we are going to add our Child device, so I hope you have its ID handy.  If not, go back and get it.
5. Add the ID and Name the device, the device may appear as a **nearby device**, in which case you can just click it to pre-populate the Device ID.
6. Click the **Sharing** Tab, and check the box next to the folder you set up on the Parent (Switch Saves in my case). Hit **Save.**
7. We are done with the parent, now **SWITCH OVER TO THE CHILD.**
8. ***ON THE CHILD***:  Right-Click the *Syncthing* Tray icon in your taskbar and select **Open Syncthing** if it is not open already.
9. You should now see a connection request pop-up from the parent.  Hit **+ Add Device** to add the device.
10. Hit **Save** to finish adding the device.
11. That pop-up will close and you will get notification that the device wants to share a folder now. Hit **Add.**
12. Enter the path to the save folder in Eden and hit **Save.**
    
    <aside>
    
    ***TIP***: The easiest way to do this would be to open Eden, right-click a game that has a save, hit ***Open Save Data Location,*** and then go up 1 directory.  It should contain folders with the TitleID of your games.
    
    It should look similar to this: ..*\nand\user\save\0000000000000000\EC573727F509799675F6E5112C581D7E*
    
    </aside>
    
13. *Syncthing* will now pull all the files from the Parent and store them in your local save directory.  At this point the files are in sync and alterations to one will affect the other and both can be considered “*Parents*” for other devices you want to add.  Repeat these steps for as many devices you want.
