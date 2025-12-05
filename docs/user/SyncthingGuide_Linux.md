# Backing Up/Syncing Eden Game Saves

Use this guide for when you want to configure automated backup/syncing of your Eden save files using [*Syncthing*](https://syncthing.net/) on Linux.

**Click [Here](https://evilperson1337.notion.site/Backing-Up-Syncing-Eden-Game-Saves-2b057c2edaf680fc8a28eba5a05fd7a3) for a version of this guide with images & visual elements.**

---

### Pre-Requisites

- Read the [*Syncthing General Guide*](./SyncthingGuide_General.md).
- Eden already installed, configured, and functioning.

---

## Steps

<aside>

***WARNING***: You should manually back up your save files before proceeding with this guide.  If you incorrectly perform the steps, you risk losing them!

</aside>

<aside>

***NOTE***: I am using Linux Mint for my guides, but the steps should translate pretty easily to your distro.  I ***hope*** that if you are running Linux you know the basic operations.  Steam Deck users should follow the guide specific to that platform.

</aside>

### Downloading and Installing Syncthing

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

---

### Configuring this Machine as a Parent

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

---

### Configuring this Machine as a Child

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