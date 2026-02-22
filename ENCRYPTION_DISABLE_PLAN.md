# Encryption/Decryption Disable Plan

## Overview
This document outlines the plan to comment out (not delete) ROM encryption/decryption code and prompt users to use pre-decrypted games.

## User Request
> "all the stuff that decrypts roms etc with keys, I want you to comment out (but not removed/delete), and prompt the user to use pre-decrypted games with the emulator, which means games would probably be in folders or in .bin, .nso, or other file types"

## Supported Pre-Decrypted Formats
- **.nso** - Native Shared Object (already decrypted executables)
- **.bin** - Binary executable files
- **Folders** - Deconstructed ROM directories (ExeFS/RomFS)
- **.nro** - Nintendo Relocatable Object files
- **.kip** - Kernel Initial Process files

## Files Requiring Changes

### High Priority - User-Facing (UI/UX)

#### 1. **src/suyu/main.cpp** + **main.ui**
- Lines 2574-2644: `OnInstallDecryptionKeys()` function
- Action: Comment out key installation dialog
- Add popup: "Please use pre-decrypted games (.nso, .bin, folders)"

#### 2. **src/suyu/main.cpp**
- Line 2443-2447: Firmware installation key check
- Action: Comment out key requirement check

### Medium Priority - Core Functionality

#### 3. **src/core/crypto/key_manager.cpp**
- Lines 651-658: `LoadFromFile()` calls for prod.keys, title.keys
- Lines 669-743: `LoadFromFile()` implementation
- Action: Comment out key file loading, return empty keys

#### 4. **src/core/crypto/key_manager.h**
- Key loading functions declarations
- Action: Add comments indicating functionality disabled

#### 5. **src/core/file_sys/content_archive.cpp**
- NCA decryption logic
- Action: Comment out decryption, pass-through raw data

#### 6. **src/core/file_sys/xts_archive.cpp**
- XTS encryption layer
- Action: Comment out encryption operations

### Low Priority - Background Services

#### 7. **src/core/hle/service/es/es.cpp**
- Title key service (GetTitleKey, ImportTicket)
- Action: Comment out, return dummy data

#### 8. **src/core/loader/nax.cpp**
- NAX (Nintendo Archive eXtended) decryption
- Action: Comment out NAX decryption logic

#### 9. **src/core/file_sys/fssystem/fssystem_aes_*.cpp**
- AES-CTR, AES-XTS storage layers
- Action: Comment out encryption, pass-through

## Implementation Strategy

### Phase 1: User-Facing Changes (HIGH PRIORITY)
1. Comment out `OnInstallDecryptionKeys()` in main.cpp
2. Add dialog prompting for pre-decrypted games
3. Remove/disable "Install Decryption Keys" menu item
4. Update error messages to mention pre-decrypted formats

### Phase 2: Key Manager (MEDIUM PRIORITY)
1. Comment out `LoadFromFile()` in key_manager.cpp
2. Make key verification functions return true/success
3. Add log messages indicating encryption disabled

### Phase 3: Decryption Layers (LOWER PRIORITY)
1. Comment out NCA decryption
2. Comment out AES encryption layers
3. Make encrypted file systems pass-through unencrypted

## Testing Approach
1. Verify UI changes don't crash
2. Test loading .nso files (should work)
3. Test loading folders (should work)
4. Ensure no crashes when encrypted formats are loaded (graceful error)

## Code Preservation
- ALL code will be commented with `/* ENCRYPTION DISABLED - [reason] */`
- Original functionality preserved for potential future revert
- Clear markers for what was disabled and why

## User Messages
When encrypted content is detected:
```
"This file appears to be encrypted. 
Please use pre-decrypted game files in the following formats:
- .nso (Native Shared Object)
- .bin (Binary executable)
- Folders (Deconstructed ROM directories)
- .nro (Nintendo Relocatable Object)
- .kip (Kernel Initial Process)

Decryption functionality has been disabled in this build."
```

## Status
- ✅ Plan created
- ⏳ Phase 1 (User-facing) - IN PROGRESS
- ⏳ Phase 2 (Key manager) - NOT STARTED
- ⏳ Phase 3 (Encryption layers) - NOT STARTED

## Notes
- This is a LARGE task affecting 100+ files
- Prioritizing user-facing changes first
- Full implementation may require multiple commits
- Testing should be done incrementally
