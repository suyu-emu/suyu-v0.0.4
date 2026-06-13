# Build System Improvements - Suyu Eclipse

This document outlines improvements applied to the build system based on the Eden emulator fork improvements.

## Date
December 14, 2025

## Key Improvements Applied

### 1. **CMake Module Path Priority**
- **What**: Updated CMakeModules path to prioritize the `eden/` subdirectory containing improved modules
- **File**: `CMakeLists.txt` (lines 14-17)
- **Why**: The Eden fork includes improvements to CPMUtil.cmake, DownloadExternals.cmake, and various Find modules
- **Impact**: Better dependency resolution and compatibility

### 2. **CPM Cache Configuration**
- **What**: Added CPM_SOURCE_CACHE configuration for centralized dependency caching
- **File**: `CMakeLists.txt` (line 19)
- **Setting**: `set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cache/cpm")`
- **Why**: Improves build performance and dependency consistency across rebuilds
- **Impact**: Faster incremental builds

### 3. **Boost Dependency Handling (Critical Fix)**
- **What**: Improved Boost package finding with fallback strategies
- **File**: `CMakeLists.txt` (lines 322-341)
- **Changes**:
  - Added `Boost_USE_STATIC_LIBS` for non-Windows/Apple platforms
  - Implemented multi-level fallback search:
    1. First attempt: Boost >= 1.79.0 with context component
    2. Fallback 1: Boost without version constraint
    3. Fallback 2: Headers-only Boost
    4. Final: Clear error message with remediation steps
- **Why**: Resolves "missing Boost package" errors that are common in fresh builds
- **Impact**: Builds work even when system Boost is slightly different version

### 4. **Boost Headers Target Guarding**
- **What**: Added defensive check for Boost::headers target
- **File**: `CMakeLists.txt` (line 736)
- **Before**: `target_link_libraries(Boost::headers INTERFACE Boost::disable_autolinking)`
- **After**: Wrapped in `if(TARGET Boost::headers)` check
- **Why**: Prevents CMake errors if Boost headers target isn't defined
- **Impact**: More robust build configuration

## Eden Fork Improvements Available

The following improvements from Eden are already integrated:
- **CPMUtil.cmake**: Advanced package management with JSON configuration
- **CPM.cmake**: Modern C++ package manager integration
- **Improved Find modules**: Better detection of system libraries
- **Architecture detection**: Platform and CPU-specific optimizations
- **Build presets**: Optimization flags for different CPU architectures

## Quick Build Verification

To verify the improvements are working:

### Linux/macOS
```bash
cd /workspaces/suyu
mkdir -p build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### Windows (MSVC)
```bash
cd /workspaces/suyu
mkdir build && cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```

## Environment Requirements

- CMake 3.22+
- Ninja 1.11+
- Boost >= 1.79.0 (with context component)
- C++20 compatible compiler (GCC 11+, Clang 12+, MSVC 2019+)

## Additional Notes

### vcpkg Configuration
The project continues to use vcpkg for dependency management on Windows. The vcpkg.json has been optimized with:
- Proper dependency ordering (cmake tools first, then libraries, then boost)
- Version overrides for consistency (boost 1.88.0)
- Feature flags for conditional dependencies

### CPM Alternative
When vcpkg is not used, the build system can fallback to system packages or CPM bundles.
The cpmfile.json can be enabled by uncommenting CPM includes in CMakeLists.txt for advanced
package management similar to Eden.

## Files Modified

1. `CMakeLists.txt`
   - Lines 1-20: Module path and CPM cache setup
   - Lines 320-341: Boost package finding with fallbacks
   - Line 736: Boost headers target guarding

## Troubleshooting

### "Boost not found" error
1. Check if Boost >= 1.79.0 is installed:
   ```bash
   dpkg -l | grep -i boost  # Linux
   brew list | grep boost   # macOS
   ```
2. If missing, install it:
   ```bash
   sudo apt-get install libboost-dev libboost-context-dev  # Ubuntu/Debian
   brew install boost  # macOS
   ```

### CMake cache issues
```bash
rm -rf build/CMakeCache.txt build/CMakeFiles
cd build && cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
```

### vcpkg issues
```bash
rm -rf .cache/cpm
rm -rf externals/vcpkg/buildtrees
rm -rf externals/vcpkg/installed
# Then rebuild
```

## References

- Eden fork: https://git.eden-emu.dev/eden-emu/eden
- Build Guide: https://git.eden-emu.dev/eden-emu/eden/src/branch/master/docs/Build.md
- Boost Documentation: https://www.boost.org/
- CMake Documentation: https://cmake.org/cmake/help/v3.22/

## Future Improvements

Consider for future iterations:
1. Full migration to CPM for cross-platform consistency
2. Integration of Eden's cpmfile.json structure
3. Build preset system for common configurations
4. Enhanced compiler flag optimizations
5. Automated dependency version management
