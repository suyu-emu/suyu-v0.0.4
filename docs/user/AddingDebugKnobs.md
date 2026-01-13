# User Handbook - Adding Debug Knobs

Debug Knobs is a 16-bit integer setting (`debug_knobs`) in the Eden Emulator that serves as a bitmask for gating various testing and debugging features. This allows developers and advanced users to enable or disable specific debug behaviors without requiring deploying of complete but temporary toggles.

The setting ranges from 0 to 65535 (0x0000 to 0xFFFF), where each bit represents a different debug feature flag.

## Index

1. [Advantages](#advantages)
2. [Usage](#usage)

   * [Accessing Debug Knobs (dev side)](#accessing-debug-knobs-dev-side)
   * [Setting Debug Knobs (user side)](#setting-debug-knobs-user-side)
   * [Bit Manipulation Examples](#bit-manipulation-examples)
3. [Examples](#examples)

   * [Example 1: Conditional Debug Logging](#example-1-conditional-debug-logging)
   * [Example 2: Performance Tuning](#example-2-performance-tuning)
   * [Example 3: Feature Gating](#example-3-feature-gating)
4. [Best Practices](#best-practices)

---

## Advantages

The main advantage is to avoid deploying new disposable toggles (those made only for testing stage, and are disposed once new feature gets good to merge). This empowers devs to be free of all frontend burocracy and hassle of new toggles.

Common advantages recap:

* **Fine-Grained Control**: Enable or disable up to 16 individual debug features independently using bit manipulation on a single build
* **Runtime Configuration**: Change debug behavior at runtime the same way as new toggles would do
* **Safe incremental development**: New debug features can be added while impact can be isolated from previous deployments

## Usage

### Accessing Debug Knobs (dev side)

Use the `Settings::getDebugKnobAt(u8 i)` function to check if a specific bit is set:

```cpp
//cpp side
#include "common/settings.h"

// Check if bit 0 is set
bool feature_enabled = Settings::getDebugKnobAt(0);

// Check if bit 15 is set
bool another_feature = Settings::getDebugKnobAt(15);
```

```kts
//kotlin side
import org.yuzu.yuzu_emu.features.settings.model.Settings

// Check if bit x is set
bool feature_enabled = Settings.getDebugKnobAt(x); //x as integer from 0 to 15
```

The function returns `true` if the specified bit (0-15) is set in the `debug_knobs` value, `false` otherwise.

### Setting Debug Knobs (user side)

Developers must inform which knobs are tied to each functionality to be tested.

The debug knobs value can be set through:

1. **Desktop UI**: In the Debug configuration tab, there's a spinbox for "Debug knobs" (0-65535)
2. **Android UI**: Available as an integer setting in the Debug section
3. **Configuration Files**: Set the `debug_knobs` value in the emulator's configuration

### Bit Manipulation Examples

To enable specific features, calculate the decimal value by setting the appropriate bits:

* **Enable only bit 0**: Value = 1 (2^0)
* **Enable only bit 1**: Value = 2 (2^1)
* **Enable bits 0 and 1**: Value = 3 (2^0 + 2^1)
* **Enable bit 15**: Value = 32768 (2^15)

## Examples

### Example 1: Conditional Debug Logging

```cpp
void SomeFunction() {
    if (Settings::getDebugKnobAt(0)) {
        LOG_DEBUG(Common, "Debug feature 0 is enabled");
        // Additional debug code here
    }
    
    if (Settings::getDebugKnobAt(1)) {
        LOG_DEBUG(Common, "Debug feature 1 is enabled");
        // Different debug behavior
    }
}
```

### Example 2: Performance Tuning

```cpp
bool UseOptimizedPath() {
    // Skip optimization if debug bit 2 is set for testing
    return !Settings::getDebugKnobAt(2);
}
```

### Example 3: Feature Gating

```cpp
void ExperimentalFeature() {
    static constexpr u8 EXPERIMENTAL_FEATURE_BIT = 3;
    
    if (!Settings::getDebugKnobAt(EXPERIMENTAL_FEATURE_BIT)) {
        // Fallback to stable implementation
        StableImplementation();
        return;
    }
    
    // Experimental implementation
    ExperimentalImplementation();
}
```

## Best Practices

* This setting is intended for development and testing purposes only
* Knobs must be unwired before PR creation
* The setting is per-game configurable, allowing different debug setups for different titles
