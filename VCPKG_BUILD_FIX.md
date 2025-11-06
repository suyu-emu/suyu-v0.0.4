# Vcpkg Build Issues Resolution

This document describes the fixes applied to resolve vcpkg build issues with boost dependencies and missing vcpkg-cmake configuration files.

## Issues Resolved

### 1. Missing vcpkg-cmake Configuration Files
**Problem**: CMake Error: include could not find requested file: `vcpkg_installed/x64-windows/share/vcpkg-cmake/vcpkg-port-config.cmake`

**Root Cause**: The boost-cmake package was attempting to build before vcpkg-cmake tools were properly installed, causing dependency ordering issues.

**Solution**: 
- **Dependency Reordering**: Restructured `vcpkg.json` to install vcpkg-cmake tools first, followed by non-boost dependencies, then boost packages last
- **Version Locking**: Added explicit version overrides for vcpkg-cmake (2024-04-23) and vcpkg-cmake-config (2024-05-23) to ensure stability
- **Baseline Alignment**: Maintained vcpkg baseline `01f602195983451bc83e72f4214af2cbc495aa94` for compatibility
- **Build Script**: Created comprehensive fix script `scripts/fix-vcpkg-build.ps1` for automated resolution

### 2. Boost Version Conflicts
**Problem**: Version conflict for boost-cobalt requesting 1.80.0 but only 1.84.0+ available

**Solution**:
- Added comprehensive version overrides for all boost components to ensure consistency at version 1.88.0
- Updated vcpkg baseline to support boost 1.88.0 ecosystem
- Added explicit override for boost-cobalt to version 1.88.0

### 3. Outdated Vcpkg Baseline
**Problem**: The vcpkg baseline `b2cb0da531c2f1f740045bfe7c4dac59f0b2b69c` was too old to support boost 1.88.0

**Solution**:
- Updated baseline in `vcpkg-configuration.json` to `01f602195983451bc83e72f4214af2cbc495aa94`
- Updated GitHub Actions workflow to use the same vcpkg commit
- Maintained registry configuration for boost packages

### 4. Inefficient vcpkg Clone in GitHub Actions
**Problem**: The workflow was using `git fetch --unshallow` after a shallow clone, which fetches the entire vcpkg repository history. This is extremely time-consuming for large repositories and can cause timeouts.

**Solution**:
- Replaced the inefficient clone approach with targeted fetch: `git fetch --depth 1 origin <commit>`
- This fetches only the specific commit needed (01f602195983451bc83e72f4214af2cbc495aa94) instead of the entire history
- Significantly reduces clone time from several minutes to seconds
- Prevents timeout issues in CI/CD pipelines

## Files Modified

### 1. `vcpkg.json`
- **Dependency Reordering**: Moved `vcpkg-cmake` and `vcpkg-cmake-config` to the beginning of dependencies array
- **Logical Grouping**: Organized dependencies as: cmake tools → non-boost libraries → boost packages
- **Version Overrides**: Added explicit version constraints for vcpkg-cmake tools:
  - vcpkg-cmake: 2024-04-23
  - vcpkg-cmake-config: 2024-05-23
- **Maintained**: All existing boost version overrides at 1.88.0

### 2. `vcpkg-configuration.json`
- **Baseline**: Maintained `01f602195983451bc83e72f4214af2cbc495aa94` for stability
- **Registry Configuration**: Preserved boost-specific registry settings
- **Overlay Ports**: Maintained custom overlay configuration

### 3. `.github/workflows/cmake-multi-platform.yml`
- **Verified**: vcpkg checkout commit matches baseline (01f602195983451bc83e72f4214af2cbc495aa94)
- **Optimized Clone**: Changed from `git fetch --unshallow` to `git fetch --depth 1 origin <commit>` to fetch only the specific commit needed, significantly reducing clone time and avoiding timeouts
- **Build Process**: Maintained existing build configuration with proper vcpkg integration

### 4. New Build Scripts
- **`scripts/fix-vcpkg-build.ps1`**: Comprehensive automated fix script that:
  - Cleans existing boost installations
  - Clears vcpkg cache and buildtrees
  - Reinstalls dependencies in correct order
  - Provides troubleshooting guidance
- **Enhanced**: Existing `scripts/clean-boost.ps1` for boost-specific cleanup

## Quick Fix Instructions

### Option 1: Automated Fix (Recommended)
Run the comprehensive fix script that handles all steps automatically:
```powershell
.\scripts\fix-vcpkg-build.ps1
```

### Option 2: Manual Steps
If you prefer manual control or the automated script fails:

1. **Clean boost components**:
   ```powershell
   .\scripts\clean-boost.ps1
   ```

2. **Clear vcpkg cache**:
   ```cmd
   rmdir /s /q vcpkg\buildtrees
   rmdir /s /q vcpkg\installed\x64-windows
   ```

3. **Reinstall dependencies**:
   ```cmd
   vcpkg install --triplet x64-windows --clean-after-build
   ```

## Build Process

The updated dependency order ensures the following installation sequence:

1. **Phase 1 - CMake Tools**: vcpkg-cmake, vcpkg-cmake-config
2. **Phase 2 - Core Libraries**: cpp-httplib, fmt, llvm, etc.
3. **Phase 3 - Boost Packages**: All boost-* components with consistent 1.88.0 versions

This prevents the "missing vcpkg-cmake configuration files" error by ensuring cmake tools are available before boost packages attempt to build.

## Verification Steps

### Automated Verification
The `fix-vcpkg-build.ps1` script includes built-in verification and will report success/failure.

### Manual Verification
1. **Verify vcpkg-cmake installation**:
   ```cmd
   dir vcpkg_installed\x64-windows\share\vcpkg-cmake\
   ```
   Should show: `vcpkg-port-config.cmake` and other cmake files

2. **Check boost installation**:
   ```cmd
   vcpkg list | findstr boost
   ```
   Should show all boost packages at version 1.88.0

3. **Test project build**:
   ```cmd
   cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

## Rollback Instructions

If these changes cause issues, you can rollback by:

1. Reverting `vcpkg-configuration.json` baseline to `b2cb0da531c2f1f740045bfe7c4dac59f0b2b69c`
2. Removing the additional boost version overrides from `vcpkg.json`
3. Reverting the GitHub Actions workflow vcpkg commit
4. Cleaning and reinstalling vcpkg dependencies

## Additional Notes

- The updated baseline is compatible with existing custom overlays (dynarmic, llvm, renderdoc-api)
- All existing features (suyu-tests, web-service, android) remain functional
- The changes maintain Windows x64 target platform compatibility
- CI/CD pipeline should work without additional modifications

## Troubleshooting

If you still encounter issues:

1. Ensure you're using the latest version of vcpkg
2. Clear vcpkg cache: `vcpkg remove --outdated`
3. Verify your vcpkg installation is not corrupted
4. Check that all submodules are properly initialized
5. Ensure you have the required Visual Studio components installed

For additional help, refer to the vcpkg documentation: https://learn.microsoft.com/vcpkg/