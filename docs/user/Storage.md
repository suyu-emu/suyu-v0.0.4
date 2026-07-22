# User Handbook - Data, savefiles and storage

## Cheats

Cheats can be placed in the two manners:

**Method 1**:
- Right click on "Open mod directory"
- Create a file called `cheat_mycheat.txt` (the file must start with `cheat_` to be recognized as one)
- Write the cheat into said text file
- Done

This method doesn't account for build IDs, so any cheats for previous builds will not be automatically filtered out.

**Method 2**:
- Right click on "Open mod directory"
- Create a folder called `My cheat`
- Inside said folder create another one: `cheats`
- Then inside said folder create a file with the build ID in hexadecimal, lowercase, for example `1234FBCDE0000000.txt`
- Write the cheat into said text file
- Done

To delete a cheat simply do the process backwards (delete the folder/file).

## Savefiles

The method to access save data is simple:
- Right click on "Open save directory"

## Maintaining a library

### ZFS

One of the best ways to keep a gallery of archives is using a ZFS pool compressed with zstd.
```sh
sudo zfs create zroot/switch`
sudo zfs set compression=zstd zroot/switch
```
A new ZFS dataset will then be available on `/zroot/switch`. From which then can be added as a normal game directory. Remember to ensure proper permissions with `chmod 755 /zroot/switch/*`.
