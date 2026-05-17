# Coding guidelines

These are **not** stylistic guidelines, they're, for the most part, suggestions on how to architecture new systems or improve upon the existing codebase.

# Foreword

Don't try to micro-optimize out of the get go, while yes, most of the code is pretty, subpar, most of these are aftertoughts and details that can be glossed over **generally**.

Architectural issues are more important, for example an API returning a `std::string` is not as efficient as one that operates on `std::string_view` directly (cost of constructing an `std::string` w/o small-string optimization and all of that).

Regardless of the details, try to keep things simple. As a general rule of thumb.

# C++ guidelines

Everyone has their own way of viewing good/bad C++ practices, my general outline:

- At your disposal you may use `boost::container::static_vector<>` (beware it has a ctor/initialization cost which goes up the more elements you add).
    - Or you may use `boost::container::small_vector<>` (which has an initialization cost as well, and will use extra book-keeping for heap, try to keep a balance).
- Don't use `[[likely]]` or `[[unlikely]]`; PGO builds exist for that.
- Don't use inline assembly to try to outsmart the compiler unless you're 100% sure the assembly you're writing is actually good.
    - And if so, try to restructure your C++ code so the compiler vectorizes it/makes it better, right?
    - Or if that fails, use intrinsics instead of raw `asm volatile`.
- Use `std::optional<>` instead of `std::unique_ptr<>` if possible.
    - `std::unique_ptr<>` carries indirection cost due to it being memory allocated on the heap.
    - It isn't often that objects that contain `std::unique_ptr<>`, are allocated on the heap themselves, allocating even more things on the heap seems redundant.
- Avoid `std::recursive_mutex` at all costs.
    - It's basically implemented as a linked list most of the time and has HEAVY performance penalties.
- Exploit the fact `std::atomic<uint32_t>/std::atomic<int32_t>` is basically free on most arches that matter.
    - In x86_64, an atomic `uint32_t` is basically `mov [m32], r32`, which is essentially free/cheap.
- Avoid template parameters unless you really need them.
    - For small inlineable functions this is fine, for more complex ones, please consider the generated assembly.
- Dont make your own memcpy/memset/strcpy/strncpy/etc.
    - Seriously DON'T DO THIS. You will NOT beat the compiler.
    - Nor 30 years of writing optimized `mem*`.
    - If your code is slow, don't blame `mem*`, blame your code.
- Try to avoid using `virtual` since vtable indirection has a cost
- Avoid `dynamic_cast` and `typeid` at all costs.
    - The reason is because the project has `-fno-rtti` disabled by default, due to the costs of dynamic polymorphism.
- Always copy-on-value for objects with `sizeof(void *) >= sizeof(T) * 2`, i.e objects sized as 2 pointers or less, for bigger objects you can use ref/pointer as usual.
- Try using move semantics instead of references, whenever possible.
- Remember function parameters are extremelly cheap as fuck, don't be afraid to place upto 8 parameters on a given function.
- Don't save a reference in structures of a parent object, i.e:
    ```c++
    struct Child {
        Parent& parent;
        void Mehod() {
            parent.Something();
        }
    };
    ```
    - Instead you can do the following:
    ```c++
    struct Child {
        void Mehod(Parent& parent) {
            parent.Something();
        }
    };
    ```
    - This reduces the amount of pointers you have lying around, and also works better because of the aforementioned cheapness of parameter functions.

# Engineering guidelines

Coding isn't also writing stuff but architecturing stuff, consider the following:

- Try to reduce dependency on... dependencies
    - While some dependencies are useful `boost::container` and `fmt` to name a few, remember each dependency added incurs a cost.
    - It may also be subpar with a hand rolled implementation, biggest exemplar of this is `spirv-tools` providing subpar SPIRV optimizations in comparison to the in-house optimizer.
- Try to rely less on indirection for architecturing systems
    - If the underlying HLE kernel emulation requires it, try making a solution that keeps things local
        - For example, there isn't a need for file descriptors to each be a pointer, when they could be a fixed table size with elements that may be emplaced at will.
