# Backing Up/Syncing Eden Game Saves

Use this guide for when you want to configure automated backup/syncing of your Eden save files using [*Syncthing*](https://syncthing.net/).

**Click [Here](https://evilperson1337.notion.site/Backing-Up-Syncing-Eden-Game-Saves-2b357c2edaf68000b40cfab2c2c3dc0a) for a version of this guide with images & visual elements.**

---

### Pre-Requisites

- Eden already installed, configured, and functioning.
- Devices to run Syncthing on.
- Ability to allow a program to communicate through the firewall of your device.

---

## Platform Specific Setup Guides

- [*Windows*](./SyncthingGuide_Windows.md)
- *MacOS  (Coming Soon)*
- *Steam Deck  (Coming Soon)*
- *Android (Coming Soon)*
- [*Linux*](./SyncthingGuide_Linux.md)

---

## A Few Notes Before You Proceed

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

---

## Overview

Rather than giving a breakdown of all the platforms and configurations, those will be in the platform’s specific guides - this will serve as a general overview of Syncthing.

---

### What is Syncthing Anyway?

Syncthing is a continuous file synchronization program (in the layman’s - make sure 2 or more systems with the same files are always up to date).  This is perfect for game saves where we would want to play on 1 device, save our game, and then continue playing it on another device.  This technology is what Epic/Steam/etc. use to allow you to do this on games run through their respective services.  Syncthing is an open source implementation of this technology that you control, rather than relying on a 3rd party.  This has a few key benefits, most notably - better security, privacy, and speed (when on your LAN).

---

### What are some common issues?

Syncthing is fairly robust and doesn’t have many issues luckily, but there are some things you should watch out for (almost all of them a user issue).

- Sync conflicts
    - If for whatever reason you update the same file on 2 different machines, the system does not know which updated file is considered the one to sync across.  This results in a ***sync conflict*** where it may not sync the files as you would expect.  Worst case scenario, this can result in your save progress being lost if you are not careful.  When one of these occurs, it will create a copy of the file and store it with a specific name, like this example, *Paper Mario.sync-conflict-20251102-072925-TZBBN6S.srm.* To resolve this, you must remove the other files and remove the *.sync-conflict-<TIMESTAMP>-<Syncthing Device ID>* from the file name of the file you want to keep.
- Accidental Deletions
    - If you delete a file from one of the devices, it will also remove the file on the other devices when they perform a sync so be careful when doing this.