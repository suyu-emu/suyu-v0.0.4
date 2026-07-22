# Eden Migration Analysis for suyu

## Overview
This document analyzes the Eden emulator improvements and provides recommendations for selective migration to suyu.

## Key Eden Improvements Identified

### 1. Game-Specific Override System (MIGRATED ✓)
**Location:** `src/core/core.cpp`
**Description:** Eden added a `LoadOverrides()` function that allows platform and GPU-specific game overrides. This is particularly useful for:
- Android/Mali GPU-specific fixes
- Per-game settings adjustments
- Platform-specific workarounds

**Status:** ✓ Migrated to suyu in this PR
- Added `program_id` member to System::Impl
- Added `LoadOverrides()` function with examples for PC and Android
- Integrated call to LoadOverrides after program ID is read

### 2. Windows Macro Safety Fix (MIGRATED ✓)
**Location:** `src/core/core.cpp`
**Description:** Changed `std::min()` to `(std::min)()` to prevent Windows macro expansion issues.

**Status:** ✓ Migrated - prevents potential build issues on Windows

### 3. CMakeLists.txt Improvements
**Differences Found:**
- Eden uses `add_compile_definitions()` instead of `add_definitions()` (more modern CMake)
- Removed `/experimental:module-` flag
- Changed `/WX` (warnings as errors) to `/WX-` (more lenient)
- Better debug info handling with `/Z7` flag
- Added Dynarmic configuration at the top level

**Recommendation:** Consider these CMake modernizations in a separate PR to avoid breaking existing builds

### 4. Network Singleton Pattern
**Location:** `src/core/core.cpp`
**Description:** Eden changed from instance variable `room_network` to static `Network::GetRoomMember()`

**Status:** Not migrated - would require broader refactoring across multiple files

### 5. Microprofile Removal
**Description:** Eden removed microprofile integration
**Status:** Not migrated - this is a significant architectural change that needs careful consideration

## Files with Extensive Overlaps

Based on `eden_conflicts_20251103_153257.txt`, these file categories have significant conflicts:
1. **Android UI Components** (~200+ files)
2. **Core Emulation** (core/, video_core/, audio_core/)
3. **Dynarmic JIT** (dynarmic/ subdirectory)
4. **Build System** (CMakeLists.txt files)

## Recommendations

### Immediate Actions (Completed)
- [x] Fix .coderabbit.yaml syntax error
- [x] Migrate LoadOverrides system for game-specific fixes
- [x] Apply Windows macro safety fix

### Short-term (Next Steps)
1. **Test the build** - Ensure migrated changes compile on Windows with vcpkg
2. **Review Android improvements** - Eden has significant Android UI enhancements
3. **Document game overrides** - Create a guide for developers to add game-specific fixes

### Medium-term
1. **Selective file migration** - Use `diff` to identify specific bug fixes in:
   - `src/video_core/` - Graphics rendering improvements
   - `src/core/hle/service/` - HLE service implementations
   - `src/core/file_sys/` - File system handling

2. **CMake modernization** - Apply Eden's CMake improvements in stages:
   - Phase 1: Switch to `add_compile_definitions()`
   - Phase 2: Update compiler flags
   - Phase 3: Reorganize dependency ordering

3. **Network refactoring** - Consider Eden's singleton pattern for network management

### Long-term
1. **Continuous Eden monitoring** - Set up automated diffing to track new Eden improvements
2. **Establish merge strategy** - Define which Eden changes align with suyu goals
3. **Automated testing** - Ensure game compatibility doesn't regress

## Migration Strategy

### Safe Migration Process
1. **Isolate changes** - Cherry-pick specific improvements, not wholesale file replacements
2. **Test incrementally** - Build and test after each migration
3. **Document rationale** - Explain why each Eden feature is or isn't migrated
4. **Preserve branding** - Keep SUYU branding (don't blindly copy YUZU references from Eden)

### Red Flags to Avoid
- Don't migrate branding changes (Eden uses "YUZU" internally)
- Don't break existing custom suyu features
- Don't migrate without understanding the change
- Don't skip testing on target platforms (Windows primarily)

## Build Requirements

### Current Status
- **Build System:** CMake 3.22+ with vcpkg
- **Primary Platform:** Windows (x64-windows)
- **Dependencies:** Managed via vcpkg.json
- **CI/CD:** GitHub Actions (cmake-multi-platform.yml)

### Build Notes
- vcpkg baseline: `01f602195983451bc83e72f4214af2cbc495aa94`
- Boost version: 1.88.0
- Known issues documented in VCPKG_BUILD_FIX.md

## Open Issues Integration

### Issue #57: Eden Improvements
**Status:** Partially addressed
- Fixed .coderabbit.yaml parsing
- Migrated game override system
- Additional Eden improvements identified but not yet migrated

### Issue #43: NEW UI
**Status:** Not addressed in this PR
**Recommendation:** Eden has significant Android UI improvements that could inform this work

### Issue #21: Nintendo Library
**Status:** Not addressed in this PR
**Recommendation:** Separate issue requiring integration work, not related to Eden migration

## Conclusion

This initial migration focused on safe, high-value improvements from Eden:
1. Game-specific override capability
2. Windows build safety fixes

Further Eden integration should be done incrementally with thorough testing to ensure suyu maintains its stability and unique features while benefiting from Eden's bug fixes.

## Eden Source Folders

**Status:** The Eden source folders (`externals/eden-src/` and `CMakeModules/eden/`) were used for initial analysis and have been removed from the codebase after migration.

All key improvements from Eden have been:
- Documented in `EDEN_IMPROVEMENTS_BACKLOG.md`
- Selectively migrated (game override system, Windows safety fixes)
- Analyzed and prioritized for future work

For future Eden comparisons, clone the repository directly from https://git.eden-emu.dev/eden-emu/eden

## Testing Checklist

Before merging any Eden improvements:
- [ ] Code compiles on Windows x64
- [ ] No regression in game compatibility
- [ ] Custom suyu features still work
- [ ] CI/CD pipeline passes
- [ ] No new compiler warnings introduced
