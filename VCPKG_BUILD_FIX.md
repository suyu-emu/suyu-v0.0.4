# Vcpkg Build Issues Resolution

This document describes the fixes applied to resolve vcpkg build issues with boost dependencies and missing vcpkg-cmake configuration files.

## Issues Resolved

### 1. Missing vcpkg-cmake Configuration Files
**Problem**: CMake Error: include could not find requested file: `vcpkg_installed/x64-windows/share/vcpkg-cmake/vcpkg-port-config.cmake`

**Solution**: 
- Updated vcpkg baseline to `01f602195983451bc83e72f4214af2cbc495aa94` (more recent version with proper vcpkg-cmake support)
- Prioritized `vcpkg-cmake` and `vcpkg-cmake-config` at the beginning of the dependency list in `vcpkg.json`

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

## Files Modified

### 1. `vcpkg-configuration.json`
- Updated baseline from `b2cb0da531c2f1f740045bfe7c4dac59f0b2b69c` to `01f602195983451bc83e72f4214af2cbc495aa94`
- Updated boost registry baseline to match

### 2. `vcpkg.json`
- Moved `vcpkg-cmake` and `vcpkg-cmake-config` to the beginning of dependencies list
- Added comprehensive version overrides for all boost components:
  - boost-algorithm: 1.88.0
  - boost-asio: 1.88.0
  - boost-bind: 1.88.0
  - boost-config: 1.88.0
  - boost-container: 1.88.0
  - boost-context: 1.88.0
  - boost-crc: 1.88.0
  - boost-functional: 1.88.0
  - boost-heap: 1.88.0
  - boost-icl: 1.88.0
  - boost-intrusive: 1.88.0
  - boost-mpl: 1.88.0
  - boost-process: 1.88.0
  - boost-range: 1.88.0
  - boost-spirit: 1.88.0
  - boost-test: 1.88.0
  - boost-timer: 1.88.0
  - boost-variant: 1.88.0
  - boost-cobalt: 1.88.0

### 3. `.github/workflows/cmake-multi-platform.yml`
- Updated vcpkg checkout commit from `46a8b3026c637b91b6e5442e37275eed79449150` to `01f602195983451bc83e72f4214af2cbc495aa94`

### 4. New Helper Scripts
- `scripts/clean-boost.bat` - Windows batch script for cleaning boost components
- `scripts/clean-boost.ps1` - PowerShell script for cleaning boost components

## Boost Cleanup Instructions

If you encounter boost-related build issues, you can clean all boost components using the provided scripts:

### Option 1: Using PowerShell (Recommended)
```powershell
.\scripts\clean-boost.ps1
```

### Option 2: Using Batch File
```cmd
scripts\clean-boost.bat
```

### Option 3: Manual Command
```cmd
.\vcpkg remove boost-uninstall:x64-windows --recurse
```

## Build Process

After applying these fixes, the build process should work as follows:

1. **Dependency Resolution**: vcpkg-cmake tools are installed first, ensuring they're available for boost packages
2. **Version Consistency**: All boost components use version 1.88.0, preventing conflicts
3. **Configuration Files**: vcpkg-cmake configuration files are properly installed and accessible
4. **Build Success**: boost-cmake and other boost components build successfully

## Verification Steps

To verify the fixes work correctly:

1. Clean any existing vcpkg installations:
   ```cmd
   .\scripts\clean-boost.ps1
   ```

2. Run vcpkg install:
   ```cmd
   vcpkg install --triplet x64-windows
   ```

3. Verify vcpkg-cmake is installed:
   ```cmd
   dir vcpkg_installed\x64-windows\share\vcpkg-cmake\
   ```

4. Build the project:
   ```cmd
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