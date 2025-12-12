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

    ***INFO***: You may get a “*Windows protected your PC”* SmartScreen message that appears.  This is just Windows Defender saying it did not recognize the application and did not run it - Eden is completely safe.  Click **More info** and then **Run anyway** to dismiss this message.
    
    </aside>
4. Eden will now launch and notify you about missing Decryption keys. Close the dialog box by hitting **OK**.
5. Navigate to **Tools → Install Decryption Keys**, navigate to the folder containing your key files and select the file, you should only be able to select one.
6. Navigate to **Tools → Install Firmware**, *Select **From Folder*** or ***From ZIP*** - depending on how your firmware is stored, navigate to where it is stored and select it.    
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
6. Navigate to **Tools → Install Decryption Keys**, navigate to the folder containing your ***prod.keys*** file and select the file and hit **Open**.
7. Navigate to **Tools → Install Firmware →** *Select **From Folder*** or ***From ZIP*** - depending on how your firmware is stored, navigate to where it is stored and select it.
8. Double-Click the main window to add the folder containing your games.
9. Go to *Emulation > Configure > Input* and set up your controller. Click **OK** to close the dialog window.
10. Double-Click a game to run it.
