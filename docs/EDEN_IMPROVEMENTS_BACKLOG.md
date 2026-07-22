# Additional Eden Improvements Analysis

## Overview
This document catalogs additional Eden emulator improvements discovered during analysis that are recommended for future migration to suyu.

## Priority Classification
- **P0 (Critical)**: Already migrated in current PR
- **P1 (High)**: Should migrate soon - clear benefits, low risk
- **P2 (Medium)**: Beneficial but requires more work/testing
- **P3 (Low)**: Consider for long-term architectural improvements

---

## P0: Already Migrated ✓

### 1. Game-Specific Override System
**File:** `src/core/core.cpp`
**Status:** ✓ Migrated
**Benefit:** Enables per-game fixes for problematic titles, especially on Android

**Details:**
```cpp
void LoadOverrides(u64 programId) const {
    // Platform and GPU-specific game overrides
    // Android/Mali GPU workarounds
    // Program ID-based configuration
}
```

### 2. Windows Macro Safety
**File:** `src/core/core.cpp`
**Status:** ✓ Migrated
**Change:** `std::min()` → `(std::min)()`
**Benefit:** Prevents Windows min/max macro expansion issues

---

## P1: High Priority - Recommended for Next PR

### 1. Configurable GPU Timing
**File:** `src/video_core/gpu.cpp`
**Impact:** High - Performance tuning flexibility

**Current Code (Suyu):**
```cpp
if (Settings::values.use_fast_gpu_time.GetValue()) {
    gpu_tick /= 256;  // Hardcoded divisor
}
```

**Eden Improvement:**
```cpp
if (Settings::values.use_fast_gpu_time.GetValue()) {
    gpu_tick /= (u64)(128 * std::pow(2,
        static_cast<u32>(Settings::values.fast_gpu_time.GetValue())));
}
```

**Benefits:**
- Configurable timing divisor per system capability
- Better performance tuning options
- User can adjust based on their hardware

**Required Changes:**
1. Add `fast_gpu_time` setting to `src/common/settings.h`:
```cpp
RangedSetting<u8> fast_gpu_time{
    linkage, 1, 0, 4, "fast_gpu_time",
    Category::RendererAdvanced, Specialization::Default,
    true, true
};
```
2. Update `src/video_core/gpu.cpp` with configurable formula
3. Add UI control for the setting (Qt and Android)

**Testing:**
- Verify on various games
- Check performance impact
- Test with different divisor values (0-4)

**Estimated Effort:** 2-4 hours

---

### 2. Logging System Improvements
**File:** `src/common/logging/log.h`
**Impact:** Medium - Better debugging capabilities

**Eden Changes:**

#### a) Namespace Safety
**Current:**
```cpp
#define LOG_DEBUG(log_class, ...) \
    Common::Log::FmtLogMessage(...)
```

**Eden:**
```cpp
#define LOG_DEBUG(log_class, ...) \
    ::Common::Log::FmtLogMessage(...)  // Fully qualified
```

**Benefit:** Prevents namespace conflicts in complex includes

#### b) Enhanced Format Support
**Current:** `#include <fmt/format.h>`
**Eden:** `#include <fmt/ranges.h>`

**Benefit:** Adds support for formatting STL containers (vectors, maps, etc.)

**Example:**
```cpp
std::vector<int> values = {1, 2, 3};
LOG_INFO(Core, "Values: {}", values);  // Now works!
```

#### c) Simplified Path Handling
**Eden Removed:** `TrimSourcePath()` function

**Benefit:**
- Simpler code
- Full paths aid debugging in complex projects
- Reduces cognitive overhead

**Required Changes:**
1. Update `src/common/logging/log.h`
2. Test that log output is acceptable with full paths
3. Update any log parsing tools if needed

**Estimated Effort:** 1-2 hours

---

## P2: Medium Priority - Plan for Future

### 1. CMake Modernization
**Files:** Various `CMakeLists.txt`
**Impact:** Build system quality

**Eden Changes:**

#### a) Modern Definitions
```cmake
# Old (Suyu):
add_definitions(-DNOMINMAX)

# New (Eden):
add_compile_definitions(NOMINMAX)
```

**Benefits:**
- Modern CMake 3.12+ best practice
- Better scoping (target-specific)
- Clearer semantics

#### b) Debug Info Handling
```cmake
# Old (Suyu):
if (USE_CCACHE OR SUYU_USE_PRECOMPILED_HEADERS)
    add_compile_options(/Z7)
endif()

# New (Eden):
if (WIN32 AND (CMAKE_BUILD_TYPE STREQUAL "Debug" OR
               CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO ...)
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG ...)
endif()
```

**Benefits:**
- More targeted debug info control
- Better conditional logic
- Improved build reliability

#### c) Warning Level Adjustments
```cmake
# Eden made /WX- (warnings not errors) more lenient
# Allows faster iteration during development
```

**Migration Plan:**
1. **Phase 1:** Switch to `add_compile_definitions()` (low risk)
2. **Phase 2:** Update debug info handling (test on Windows)
3. **Phase 3:** Review warning flags (team decision needed)

**Estimated Effort:** 4-8 hours across all phases

---

### 2. Android Drawable Resources
**Location:** `src/android/app/src/main/res/drawable/`
**Status:** Partial migration (8 eden_*.xml files exist)

**Eden Drawables Present:**
- eden_background_gradient.xml
- eden_button_primary_background.xml
- eden_card_background.xml
- eden_card_elevated_background.xml
- eden_card_elevated_selector.xml
- eden_dialog_background.xml
- eden_gradient_border.xml
- eden_list_item_selector.xml

**Opportunity:**
These Eden UI components could inform Issue #43 (NEW UI)

**Actions:**
1. Audit existing eden_*.xml files
2. Compare with Eden's latest versions
3. Consider rebranding eden_* → suyu_* if keeping
4. Integrate into NEW UI design (Issue #43)

**Estimated Effort:** Coordinate with UI redesign work

---

## P3: Low Priority - Long-term Architectural

### 1. Network Singleton Pattern
**File:** `src/core/core.cpp`
**Impact:** Architectural change

**Current (Suyu):**
```cpp
struct System::Impl {
    Network::RoomNetwork room_network;  // Instance member
    // ...
};

if (auto room_member = room_network.GetRoomMember().lock()) {
    // Use room_member
}
```

**Eden:**
```cpp
// No instance member, uses static/singleton
if (auto room_member = Network::GetRoomMember().lock()) {
    // Direct access
}
```

**Benefits:**
- Cleaner architecture
- Easier access from multiple subsystems
- Reduced coupling

**Risks:**
- Breaking change
- Requires updating all network access points
- Potential thread safety considerations

**Required Changes:**
1. Refactor Network::RoomNetwork to singleton
2. Update all callsites (search for "room_network")
3. Ensure thread safety
4. Test multiplayer functionality

**Estimated Effort:** 8-16 hours + extensive testing

---

### 2. Microprofile Removal
**Files:** Various (core, video_core, etc.)
**Impact:** Large - removes profiling infrastructure

**Eden Changes:**
- Removed `#include "common/microprofile.h"`
- Removed `MICROPROFILE_DEFINE(...)` macros
- Removed `MICROPROFILE_TOKEN(...)` calls
- Removed microprofile CPU tracking arrays

**Benefits:**
- Cleaner code
- Reduced dependencies
- Potentially faster compilation

**Risks:**
- Loss of profiling capability
- May need replacement profiling tool
- Performance debugging becomes harder

**Decision Needed:**
Does suyu need built-in profiling? Options:
1. Keep microprofile (current state)
2. Remove like Eden (cleaner but lose profiling)
3. Replace with alternative (Tracy, etc.)

**Estimated Effort:** 16+ hours + team decision

---

## Branding Changes (NOT Recommended)

### What NOT to Migrate

Eden reverted many `SUYU_*` constants back to `YUZU_*`:
- `SUYU_PAGESIZE` → `YUZU_PAGESIZE`
- `SUYU_PAGEMASK` → `YUZU_PAGEMASK`
- `SUYU_PAGEBITS` → `YUZU_PAGEBITS`
- Header comments reference "yuzu" not "eden"

**Reason:** Eden is poorly rebranded

**suyu Policy:**
- Keep SUYU branding
- Don't blindly copy Eden's YUZU references
- Maintain project identity

---

## Migration Workflow

### For Each Eden Improvement:

1. **Identify** - Document the change in this file
2. **Analyze** - Understand benefits and risks
3. **Plan** - Create detailed migration plan
4. **Test** - Implement in feature branch
5. **Validate** - Run full test suite
6. **Review** - Code review + testing
7. **Merge** - Only after validation

### Testing Requirements:

For each migration:
- [ ] Code compiles on Windows (primary target)
- [ ] No new compiler warnings
- [ ] Game compatibility unchanged
- [ ] Performance impact measured (if applicable)
- [ ] CI/CD pipeline passes
- [ ] Manual testing on key games

---

## Tracking Eden Updates

### Recommended Process:

1. **Periodic Checks** (Monthly or per release)
   - Check https://git.eden-emu.dev/eden-emu/eden for updates
   - Clone Eden repo and run `diff` on key files
   - Update this document with findings

2. **Automated Diffing Script** (Future)
   ```bash
   # Clone Eden repository for comparison
   git clone https://git.eden-emu.dev/eden-emu/eden eden-temp

   # Compare specific directories
   diff -r src/core eden-temp/src/core > eden_core_diff.txt
   diff -r src/video_core eden-temp/src/video_core > eden_video_diff.txt

   # Clean up
   rm -rf eden-temp
   ```

3. **Issue Tracking**
   - Create issues for valuable Eden improvements
   - Label as "eden-migration"
   - Prioritize based on impact

---

## Quick Reference: Eden Source

**Note:** The Eden source folders were used for initial analysis and have been removed after migration. All key improvements are documented in this file.

**Repository:** https://git.eden-emu.dev/eden-emu/eden
**Analysis Completed:** 2025-11-04
**Status:** Analysis complete, folders removed from codebase

**For Future Updates:**
Clone the Eden repository directly from upstream to compare against latest changes.

---

## Questions & Decisions Needed

### For Project Maintainers:

1. **GPU Timing:** Should we add configurable fast_gpu_time? (P1)
2. **Logging:** OK to show full file paths in logs? (P1)
3. **Microprofile:** Keep, remove, or replace? (P3)
4. **Network:** Refactor to singleton worth the effort? (P3)
5. **Eden Monitoring:** Automate diff checking? (Process)

---

## Conclusion

Eden has made valuable improvements beyond the already-migrated changes. Highest priority items (P1) should be considered for the next PR, as they provide clear benefits with manageable effort.

The configurable GPU timing in particular could help users with performance issues, while the logging improvements aid debugging.

Continue selective migration using this document as a guide, always prioritizing stability and suyu's unique identity.
