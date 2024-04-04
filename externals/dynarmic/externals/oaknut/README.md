# Oaknut

*A C++20 assembler for AArch64 (ARMv8.0 to ARMv8.2)*

Oaknut is a header-only library that allows one to dynamically assemble code in-memory at runtime.

## Usage

Give `oaknut::CodeGenerator` a pointer to a block of memory. Call functions on it to emit code.

Simple example:

```cpp
#include <cstdio>
#include <oaknut/code_block.hpp>
#include <oaknut/oaknut.hpp>

using EmittedFunction = int (*)();

EmittedFunction EmitExample(oaknut::CodeGenerator& code, int value)
{
    using namespace oaknut::util;

    EmittedFunction result = code.xptr<EmittedFunction>();

    code.MOV(W0, value);
    code.RET();

    return result;
}

int main()
{
    oaknut::CodeBlock mem{4096};
    oaknut::CodeGenerator code{mem.ptr()};

    mem.unprotect();

    EmittedFunction fn = EmitExample(code, 42);

    mem.protect();
    mem.invalidate_all();

    std::printf("%i\n", fn());  // Output: 42

    return 0;
}
```

CodeGenerator also has a constructor taking two pointers. The first pointer is the memory address to write to, and the second pointer is the memory address that the code will be executing from. This allows you to write to a buffer before copying to the final destination for execution, or to have to use dual-mapped memory blocks to avoid memory protection overhead.

Below is an example of using the oaknut-provided utility header for dual-mapped memory blocks:

```cpp
#include <cstdio>
#include <oaknut/dual_code_block.hpp>
#include <oaknut/oaknut.hpp>

using EmittedFunction = ;

int main()
{
    using namespace oaknut::util;

    oaknut::DualCodeBlock mem{4096};
    oaknut::CodeGenerator code{mem.wptr(), mem.xptr()};

    const auto result = code.xptr<int (*)()>();

    code.MOV(W0, value);
    code.RET();

    mem.invalidate_all();

    std::printf("%i\n", fn());  // Output: 42

    return 0;
}
```

### Emit to `std::vector`

If you wish to merely emit code into memory without executing it, or if you are developing a cross-compiler that is not running on an ARM64 device, you can use `oaknut::VectorCodeGenerator` instead.

Provide `oaknut::VectorCodeGenerator` with a reference to a `std::vector<std::uint32_t>` and it will append to that vector.

The second pointer argument represents the destination address the code will eventually be executed from.

Simple example:

```cpp
#include <cstdint>
#include <cstdio>
#include <oaknut/oaknut.hpp>
#include <vector>

int main()
{
    std::vector<std::uint32_t> vec;
    oaknut::VectorCodeGenerator code{vec, (uint32_t*)0x1000};

    code.MOV(W0, 42);
    code.RET();

    std::printf("%08x %08x\n", vec[0], vec[1]);  // Output: d2800540 d65f03c0

    return 0;
}
```

## Headers

| Header | Compiles on non-ARM64 | Contents |
| ------ | --------------------- | -------- |
| `<oaknut/oaknut.hpp>` | Yes | Provides `CodeGenerator` and `VectorCodeGenerator` for code emission, as well as the `oaknut::util` namespace. |
| `<oaknut/code_block.hpp>` | No | Utility header that provides `CodeBlock`, allocates, alters permissions of, and invalidates executable memory. |
| `<oaknut/dual_code_block.hpp>` | No | Utility header that provides `DualCodeBlock`, which allocates two mirrored memory blocks (with RW and RX permissions respectively). |
| `<oaknut/oaknut_exception.hpp>` | Yes | Provides `OaknutException` which is thrown on an error. |
| `<oaknut/feature_detection/cpu_feature.hpp>` | Yes | Utility header that provides `CpuFeatures` which can be used to describe AArch64 features. |
| `<oaknut/feature_detection/feature_detection.hpp>` | No | Utility header that provides `detect_features` and `read_id_registers` for determining available AArch64 features. |

### Instructions

Each AArch64 instruction corresponds to one emitter function. For a list of emitter functions see:
* ARMv8.0: [general instructions](include/oaknut/impl/mnemonics_generic_v8.0.inc.hpp), [FP & SIMD instructions](include/oaknut/impl/mnemonics_fpsimd_v8.0.inc.hpp)
* ARMv8.1: [general instructions](include/oaknut/impl/mnemonics_generic_v8.1.inc.hpp), [FP & SIMD instructions](include/oaknut/impl/mnemonics_fpsimd_v8.1.inc.hpp)
* ARMv8.2: [general instructions](include/oaknut/impl/mnemonics_generic_v8.2.inc.hpp), [FP & SIMD instructions](include/oaknut/impl/mnemonics_fpsimd_v8.2.inc.hpp)

### Operands

The `oaknut::util` namespace provides convenient names for operands for instructions. For example:

|Name|Class|  |
|----|----|----|
|W0, W1, ..., W30|`WReg`|32-bit general purpose registers|
|X0, X1, ..., X30|`XReg`|64-bit general purpose registers|
|WZR|`WzrReg` (convertable to `WReg`)|32-bit zero register|
|XZR|`ZrReg` (convertable to `XReg`)|64-bit zero register|
|WSP|`WspReg` (convertable to `WRegSp`)|32-bit stack pointer|
|SP|`SpReg` (convertable to `XRegSp`)|64-bit stack pointer|
|B0, B1, ..., B31|`BReg`|8-bit scalar SIMD register|
|H0, H1, ..., H31|`HReg`|16-bit scalar SIMD register|
|S0, S1, ..., S31|`SReg`|32-bit scalar SIMD register|
|D0, D1, ..., D31|`DReg`|64-bit scalar SIMD register|
|Q0, Q1, ..., Q31|`QReg`|128-bit scalar SIMD register|

For vector operations, you can specify registers like so:

|Name|Class|  |
|----|----|----|
|V0.B8(), ...|`VReg_8B`|8 elements each 8 bits in size|
|V0.B16(), ...|`VReg_16B`|16 elements each 8 bits in size|
|V0.H4(), ...|`VReg_4H`|4 elements each 16 bits in size|
|V0.H8(), ...|`VReg_8H`|8 elements each 16 bits in size|
|V0.S2(), ...|`VReg_2S`|2 elements each 32 bits in size|
|V0.S4(), ...|`VReg_4S`|4 elements each 32 bits in size|
|V0.D1(), ...|`VReg_1D`|1 elements each 64 bits in size|
|V0.D2(), ...|`VReg_2D`|2 elements each 64 bits in size|

And you can specify elements like so:

|Name|Class|  |
|----|----|----|
|V0.B()[0]|`BElem`|0th 8-bit element of V0 register|
|V0.H()[0]|`HElem`|0th 16-bit element of V0 register|
|V0.S()[0]|`SElem`|0th 32-bit element of V0 register|
|V0.D()[0]|`DElem`|0th 64-bit element of V0 register|

Register lists are specified using `List`:

```
List{V0.B16(), V1.B16(), V2.B16()}  // This expression has type List<VReg_16B, 3>
```

And lists of elements similarly (both forms are equivalent):

```
List{V0.B()[1], V1.B()[1], V2.B()[1]}  // This expression has type List<BElem, 3>
List{V0.B(), V1.B(), V2.B()}[1]        // This expression has type List<BElem, 3>
```

You can find examples of instruction use in [tests/general.cpp](tests/general.cpp) and [tests/fpsimd.cpp](tests/fpsimd.cpp).

## Feature Detection

### CPU features

This library also includes utility headers for CPU feature detection.

One just needs to include `<oaknut/feature_detection/feature_detection.hpp>`, then call `detect_features` to get a bitset of features in a cross-platform manner.

CPU feature detection is operating system specific, and some operating systems even have multiple methods. Here are a list of supported operating systems and implemented methods:

| Operating system | Default Method |
| ---- | ---- |
| Linux / Android | [ELF hwcaps](https://www.kernel.org/doc/html/latest/arch/arm64/elf_hwcaps.html) |
| Apple | [sysctlbyname](https://developer.apple.com/documentation/kernel/1387446-sysctlbyname) |
| Windows | [IsProcessorFeaturePresent](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-isprocessorfeaturepresent) |
| FreeBSD | ELF hwcaps |
| NetBSD | machdep.cpu%d.cpu_id sysctl |
| OpenBSD | CTL_MACHDEP.CPU_ID_* sysctl |

There are alternative methods available for advanced users to specify specific methods to detect features if they wish. (See `detect_features_via_*`.)

Simple example:

```cpp
#include <cstdio>
#include <oaknut/feature_detection/feature_detection.hpp>

int main() {
    oaknut::CpuFeatures feats = oaknut::detect_features();

    std::printf("CPU supports JSCVT: %i\n", feats.has(oaknut::CpuFeature::JSCVT));
}
```

### ID registers

We also provide a crossplatform way for ID registers to be read:

| **`OAKNUT_SUPPORTS_READING_ID_REGISTERS`** | Available functionality |
| ---- | ---- |
| 0 | Reading ID registers is not supported on this operating system. |
| 1 | This operating system provides a system-wide set of ID registers, use `read_id_registers()`. |
| 2 | Per-core ID registers, use `get_core_count()` and `read_id_registers(int index)`. |

All of the above operating systems with the exception of apple also support reading ID registers, and if one prefers one can do feature detection via `detect_features_via_id_registers(*read_id_registers())`.

Simple example:

```cpp
#include <cstddef>
#include <cstdio>
#include <oaknut/feature_detection/feature_detection.hpp>

int main() {
#if OAKNUT_SUPPORTS_READING_ID_REGISTERS == 1

    oaknut::id::IdRegisters id = oaknut::read_id_registers();

    std::printf("ISAR0 register: %08x\n", id.isar0.value);

#elif OAKNUT_SUPPORTS_READING_ID_REGISTERS == 2

    oaknut::id::IdRegisters id = oaknut::read_id_registers(0);

    const std::size_t core_count = oaknut::get_core_count();
    for (std::size_t core_index = 0; core_index < core_count; core_index++) {
        std::printf("ISAR0 register (for core %zu): %08x\n", core_index, id.isar0.value);
    }

#else

    std::printf("Reading ID registers not supported\n");

#endif
}
```

## License

This project is [MIT licensed](LICENSE).
