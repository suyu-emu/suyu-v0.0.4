# Eden Fails to Launch and Does Not Leave Any Logs

**Click [Here](https://evilperson1337.notion.site/Windows-Eden-Fails-to-Launch-and-Does-Not-Leave-Any-Logs-2b057c2edaf68156b640cf1ac549870a) for a version of this guide with images & visual elements.**

---

## Error Details

*Behavior*:  Program appears not to launch or exits immediately without leaving any log entries.
*Platform(s) Affected*:
- **Windows**

**Error Log Entries:** 

```
None
```
**Example Error Message Entry in Windows Event Viewer**
```
Faulting application name: eden.exe, version: 0.0.0.0, time stamp: 0x6795dc3c
Faulting module name: ntdll.dll, version: 10.0.26100.3037, time stamp: 0x95e6c489
Exception code: 0xc0000005
Fault offset: 0x0000000000014778
Faulting process id: 0x2AF0
Faulting application start time: 0x1DB7C30D2972402
Faulting application path: C:\temp\Eden-Windows\eden.exe
Faulting module path: C:\WINDOWS\SYSTEM32\ntdll.dll
Report Id: 4c8a6e13-9637-438c-b4d0-e802d279af66
Faulting package full name: 
Faulting package-relative application ID:
```

---

## Causes

<aside>

### Issue 1: Missing C++ Redistributable

---

*Eden requires the latest C++ redistributable from Microsoft in order to run.  Like many other programs, it relies on aspects and libraries included in this runtime, without it - the program cannot run.*

1. Download the [Latest C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#visual-studio-2015-2017-2019-and-2022) from Microsoft for your machine.
2. Double Click the downloaded executable file and wait for the software to install.
3. Restart the computer.
4. Launch Eden again, if the main window appears, you are good to go.  If not, proceed to the next issue.
</aside>

<aside>

### Issue 2: Corrupted System Files

---

*A corruption of necessary system files can cause odd behaviors when Eden tries to access them.  It is a very rare case and you would likely see other programs misbehaving if this is what your issue is, but you can try if you have no other options.*

1. Launch Eden to generate a crash.
2. Confirm there are no logs created in the log directory.
    1. See the [How to Access Logs](./HowToAccessLogs.md) page for the log location if you need it.
    2. If there are any entries in here since you tried step 1, this is likely not your issue.
3. Navigate to your *Windows Event Viewer* (Start Menu → **eventvwr.msc)**. 
4. Expand **Windows Logs** and select **Application.**
    
5. Look for an entry with the Level of Error, and look for a message similar to the following
    
    ```
    Faulting application name: Eden.exe, version: 0.0.0.0, time stamp: 0x6795dc3c
    Faulting module name: ntdll.dll, version: 10.0.26100.3037, time stamp: 0x95e6c489
    Exception code: 0xc0000005
    Fault offset: 0x0000000000014778
    Faulting process id: 0x2AF0
    Faulting application start time: 0x1DB7C30D2972402
    Faulting application path: C:\temp\Eden-Windows\Eden.exe
    Faulting module path: C:\WINDOWS\SYSTEM32\ntdll.dll
    Report Id: 4c8a6e13-9637-438c-b4d0-e802d279af66
    Faulting package full name: 
    Faulting package-relative application ID:
    ```
    
6. Run a Command Prompt terminal Window as Administrator.
7. Enter the following command and wait for it to complete.  It will take a while, just be patient and do other things while it completes.
    
    ```
    DISM /Online /Cleanup-Image /RestoreHealth
    ```
    
8. Reboot your computer.
9. Launch Eden and verify it is now working.
</aside>