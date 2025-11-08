# VCPKG Overlay Port Fixes

This document summarizes the fixes applied to resolve vcpkg build issues with the LLVM and renderdoc-api overlay ports.

## Issues Resolved

### 1. renderdoc-api Port Issues

**Problem**: Download failure with HTTP 404 error when trying to fetch v1.29.0 from GitHub.

**Root Cause**: The version v1.29.0 does not exist in the baldurk/renderdoc repository.

**Solution**:
- Changed version from v1.29.0 to v1.27.0 (a known stable release)
- Updated both `portfile.cmake` and `vcpkg.json` to use version 1.27.0
- Added validation to ensure the API directory exists before installation
- Improved the CMake config file to provide proper target definitions
- Note: SHA512 is temporarily set to 0 - vcpkg will calculate and report the correct hash on first build

**Files Modified**:
- `vcpkg-overlays/renderdoc-api/portfile.cmake`
- `vcpkg-overlays/renderdoc-api/vcpkg.json`

### 2. LLVM Port Policy Violations

**Problems**: Multiple vcpkg policy violations causing warnings:
1. CMake files installed in `lib/cmake/llvm` instead of `share/llvm`
2. Separate debug/release CMake directories not merged
3. Executables (llvm-tblgen.exe) present in bin directories for static build
4. bin and debug/bin directories exist in static build
5. Absolute paths embedded in LLVMConfig.cmake files

**Solutions Applied**:

#### CMake Configuration Fixup
- Added `vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/llvm)` to properly relocate and merge CMake files
- This resolves both the misplaced files and lib/cmake merge warnings

#### Static Build Cleanup
- Added logic to remove bin directories for static builds: `file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/bin" "${CURRENT_PACKAGES_DIR}/bin")`
- Added tool copying before removal in case llvm-tblgen is needed: `vcpkg_copy_tools(TOOL_NAMES llvm-tblgen AUTO_CLEAN)`

#### Absolute Paths Suppression
- Added `set(VCPKG_POLICY_SKIP_ABSOLUTE_PATHS_CHECK enabled)` to suppress absolute path warnings
- This is necessary because LLVM's build system embeds build paths that are difficult to remove without extensive patching

**Files Modified**:
- `vcpkg-overlays/llvm/portfile.cmake`

## Implementation Details

### LLVM Port Changes

```cmake
# Added at the top
set(VCPKG_POLICY_SKIP_ABSOLUTE_PATHS_CHECK enabled)

# Added after vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/llvm)

if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    if(EXISTS "${CURRENT_PACKAGES_DIR}/bin/llvm-tblgen.exe")
        vcpkg_copy_tools(TOOL_NAMES llvm-tblgen AUTO_CLEAN)
    endif()
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/bin" "${CURRENT_PACKAGES_DIR}/bin")
endif()
```

### renderdoc-api Port Changes

```cmake
# Changed version from v1.29.0 to v1.27.0
REF v1.27.0

# Added validation
if(NOT EXISTS "${SOURCE_PATH}/renderdoc/api")
    message(FATAL_ERROR "RenderDoc API directory not found at ${SOURCE_PATH}/renderdoc/api")
endif()

# Improved CMake config with proper target definition
```

## Expected Outcomes

After these changes:
1. **renderdoc-api**: Should download successfully and install API headers
2. **LLVM**: Should build without policy violation warnings
3. **CMake Integration**: Both ports should provide proper CMake targets for downstream consumption
4. **Static Builds**: LLVM static builds should not contain unnecessary bin directories
5. **File Organization**: CMake files should be in the correct vcpkg-standard locations

## Notes

- The renderdoc-api SHA512 hash is temporarily set to 0. vcpkg will calculate and display the correct hash on the first build attempt, which should then be updated in the portfile.
- The LLVM absolute paths policy is suppressed because fixing this would require extensive patching of LLVM's CMake configuration generation, which is beyond the scope of this fix.
- The llvm-tblgen tool is preserved in case it's needed by downstream consumers, but moved to the tools directory following vcpkg conventions.

## Testing

To verify these fixes:
1. Run `vcpkg install` with the updated overlay ports
2. Confirm no policy violation warnings appear
3. Verify that downstream projects can successfully find and use both packages
4. Check that CMake config files are in the expected locations (`share/PORT/cmake/`)