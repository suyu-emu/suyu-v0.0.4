# Eden Quick Start

Use this guide to get starting using the Eden emulator.

**Click [Here](https://evilperson1337.notion.site/Eden-Quick-Start-2b057c2edaf6817b9859d8bcdb474017) for a version of this guide with images & visual elements.**

## Windows

### Pre-Requisites

- The [*latest C++ Redistributable*](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#latest-supported-redistributable-version) from Microsoft.
    - Eden will not even launch without it see [*Eden Fails to Launch*](./Troubleshoot.md) for more information.
- Firmware dumped from your console
- Keys extracted from your console
- Games dumped from your console
- Internet Connection

### Steps

1. Download either the *Stable* or *Nightly* Eden application.
    <aside>
    
    ***TIP***:  If you have questions about the requirements, architectures, or general information surrounding what release you need - see the [*Basics Guide*](./Basics.md) and [*Architectures Guide*](./Architectures.md).
    
    </aside>
2. Extract the contents to wherever you want to store the program on your computer. 
    <aside>
    
    ***TIP***: If you want to run Eden completely portable (everything is stored in the folder you extracted it to) - create a folder called **user** if it is not there by default.
    
    </aside> 
3. Run ***Eden.exe*** to launch the program.  
    
    <aside>

    ***INFO***: You may get a "*Windows protected your PC"* SmartScreen message that appears.  This is just Windows Defender saying it did not recognize the application and did not run it - Eden is completely safe.  Click **More info** and then **Run anyway** to dismiss this message.
    
    </aside>
4. Eden will now launch and notify you about missing Decryption keys. Close the dialog box by hitting **OK**.
5. Navigate to **Tools > Install Decryption Keys**, navigate to the folder containing your key files and select the file, you should only be able to select one.
6. Navigate to **Tools > Install Firmware**, *Select **From Folder*** or ***From ZIP*** - depending on how your firmware is stored, navigate to where it is stored and select it.    
7. Double-Click the main window to add the folder containing your games.
8. Go to *Emulation > Configure > Input* and set up your controller of choice. Click **OK** to close the dialog window.
9. Double-Click a game to run it.

## Steamdeck

### Pre-Requisites

- Firmware dumped from your console
- Keys extracted from your console
- Games dumped from your console
- Internet Connection

### Steps

1. Access Steam Desktop Mode.
2. Download either the *Stable* or *Nightly* Eden AppImage onto your Steam Deck and save it somewhere accessible.
    
    <aside>
        
    ***TIP***:  If you have questions about the requirements, architectures, or general information surrounding what release you need - see the [*Basics Guide*](./Basics.md) and [*Architectures Guide*](./Architectures.md).
        
    </aside>
    
3. Double-Click the Eden executable to launch the program.
    <aside>

    ***NOTE***: The first time you run the AppImage you will get a notification asking you to confirm you want to launch the program.  Hit **Continue**.

    </aside>
    
4. If you have had a different Switch emulator installed, it will detect and ask if you want to import those settings.  Make your selection to close the screen.
5. Eden will now launch and notify you about missing Encryption keys. Close the dialog box by hitting **OK**.
6. Navigate to **Tools > Install Decryption Keys**, navigate to the folder containing your ***prod.keys*** file and select the file and hit **Open**.
7. Navigate to **Tools > Install Firmware >** *Select **From Folder*** or ***From ZIP*** - depending on how your firmware is stored, navigate to where it is stored and select it.
8. Double-Click the main window to add the folder containing your games.
9. Go to *Emulation > Configure > Input* and set up your controller. Click **OK** to close the dialog window.
10. Double-Click a game to run it.

## macOS

Current macOS support is still experimental and very reliant on MoltenVK developments, plans have shifted to properly provide support for KosmicKrisp and similar new GPU endeavours, but macOS users still are bound to MoltenVK itself.

Users of macOS may wish to use [Asahi Linux](https://wiki.gentoo.org/wiki/Project:Asahi/Guide) for the rising KosmicKrisp support.

As of writing, neither macOS nor Asahi has support for NCE; additionally Asahi has extraneous paging bugs with fastmem.

### Allowing Eden to Run on MacOS

Use this guide when you need to allow Eden to run on a Mac system, but are being blocked by Apple Security policy.

**Click [Here](https://evilperson1337.notion.site/Allowing-Eden-to-Run-on-MacOS-2b057c2edaf681fea63dc81027efeffd) for a version of this guide with images & visual elements.**

---

##### Pre-Requisites

- Permissions to modify settings in MacOS

---

#### Why am I Seeing This?

Recent versions of MacOS (Catalina & newer) introduced the **Gatekeeper** security functionality, requiring software to be signed by Apple or a trusted (aka - paying) developer.  If the signature isn't on the list of trusted ones, it will stop the program from executing and display the message above.

---

#### Steps

1. Open the *System Settings* panel.
2. Navigate to *Privacy & Security*.
3. Scroll down and observe the following message under the **Security** settings.
4. Select **Open Anyway** to tell your Mac that you trust the application.
5. You will now get another window appearing to verify you want to open Eden.  Select **Open Anyway**.
6. You will be prompted for your password to authorize the request.  Enter the credentials of an account that has permission to modify settings and press **OK**.
7. Eden will now open and any subsequent launches of the program will not prompt this.
