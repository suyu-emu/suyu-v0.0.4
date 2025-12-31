# Dynarmic Design Documentation

Dynarmic is a dynamic recompiler for the ARMv6K architecture. Future plans for dynarmic include
support for other versions of the ARM architecture, having a interpreter mode, and adding support
for other architectures.

Users of this library interact with it primarily through the interface provided in
[`src/dynarmic/interface`](../src/dynarmic/interface). Users specify how dynarmic's CPU core interacts with
the rest of their system providing an implementation of the relevant `UserCallbacks` interface.
Users setup the CPU state using member functions of `Jit`, then call `Jit::Execute` to start CPU
execution. The callbacks defined on `UserCallbacks` may be called from dynamically generated code,
so users of the library should not depend on the stack being in a walkable state for unwinding.

* A32: [`Jit`](../src/dynarmic/interface/A32/a32.h), [`UserCallbacks`](../src/dynarmic/interface/A32/config.h)
* A64: [`Jit`](../src/dynarmic/interface/A64/a64.h), [`UserCallbacks`](../src/dynarmic/interface/A64/config.h)

Dynarmic reads instructions from memory by calling `UserCallbacks::MemoryReadCode`. These
instructions then pass through several stages:

1. Decoding (Identifying what type of instruction it is and breaking it up into fields)
2. Translation (Generation of high-level IR from the instruction)
3. Optimization (Eliminiation of redundant microinstructions, other speed improvements)
4. Emission (Generation of host-executable code into memory)
5. Execution (Host CPU jumps to the start of emitted code and runs it)

Using the A32 frontend with the x64 backend as an example:

* Decoding is done by [double dispatch](https://en.wikipedia.org/wiki/Visitor_pattern) in
  [`src/frontend/A32/decoder/{arm.h,thumb16.h,thumb32.h}`](../src/dynarmic/frontend/A32/decoder/).
* Translation is done by the visitors in [`src/dynarmic/frontend/A32/translate/translate_{arm,thumb}.cpp`](../src/dynarmic/frontend/A32/translate/).
  The function [`Translate`](../src/dynarmic/frontend/A32/translate/translate.h) takes a starting memory location,
  some CPU state, and memory reader callback and returns a basic block of IR.
* The IR can be found under [`src/frontend/ir/`](../src/dynarmic/ir/).
* Optimizations can be found under [`src/ir_opt/`](../src/dynarmic/ir/opt/).
* Emission is done by `EmitX64` which can be found in [`src/dynarmic/backend/x64/emit_x64.{h,cpp}`](../src/dynarmic/backend/x64/).
* Execution is performed by calling `BlockOfCode::RunCode` in [`src/dynarmic/backend/x64/block_of_code.{h,cpp}`](../src/dynarmic/backend/x64/).

## Decoder

The decoder is a double dispatch decoder. Each instruction is represented by a line in the relevant
instruction table. Here is an example line from [`arm.h`](../src/dynarmic/frontend/A32/decoder/arm.h):

    INST(&V::arm_ADC_imm,     "ADC (imm)",           "cccc0010101Snnnnddddrrrrvvvvvvvv")

(Details on this instruction can be found in section A8.8.1 of the ARMv7-A manual. This is encoding A1.)

The first argument to INST is the member function to call on the visitor. The second argument is a user-readable
instruction name. The third argument is a bit-representation of the instruction.

### Instruction Bit-Representation

Each character in the bitstring represents a bit. A `0` means that that bitposition **must** contain a zero. A `1`
means that that bitposition **must** contain a one. A `-` means we don't care about the value at that bitposition.
A string of the same character represents a field. In the above example, the first four bits `cccc` represent the
four-bit-long cond field of the ARM Add with Carry (immediate) instruction.

The visitor would have to have a function named `arm_ADC_imm` with 6 arguments, one for each field (`cccc`, `S`,
`nnnn`, `dddd`, `rrrr`, `vvvvvvvv`). If there is a mismatch of field number with argument number, a compile-time
error results.

## Translator

The translator is a visitor that uses the decoder to decode instructions. The translator generates IR code with the
help of the [`IREmitter` class](../src/dynarmic/ir/ir_emitter.h). An example of a translation function follows:

    bool ArmTranslatorVisitor::arm_ADC_imm(Cond cond, bool S, Reg n, Reg d, int rotate, Imm8 imm8) {
        u32 imm32 = ArmExpandImm(rotate, imm8);

        // ADC{S}<c> <Rd>, <Rn>, #<imm>

        if (ConditionPassed(cond)) {
            auto result = ir.AddWithCarry(ir.GetRegister(n), ir.Imm32(imm32), ir.GetCFlag());

            if (d == Reg::PC) {
                ASSERT(!S);
                ir.ALUWritePC(result.result);
                ir.SetTerm(IR::Term::ReturnToDispatch{});
                return false;
            }

            ir.SetRegister(d, result.result);
            if (S) {
                ir.SetNFlag(ir.MostSignificantBit(result.result));
                ir.SetZFlag(ir.IsZero(result.result));
                ir.SetCFlag(result.carry);
                ir.SetVFlag(result.overflow);
            }
        }

        return true;
    }

where `ir` is an instance of the `IRBuilder` class. Each member function of the `IRBuilder` class constructs
an IR microinstruction.

## Intermediate Representation

Dynarmic uses an ordered SSA intermediate representation. It is very vaguely similar to those found in other
similar projects like redream, nucleus, and xenia. Major differences are: (1) the abundance of context
microinstructions  whereas those projects generally only have two (`load_context`/`store_context`), (2) the
explicit handling of flags as their own values, and (3) very different basic block edge handling.

The intention of the context microinstructions and explicit flag handling is to allow for future optimizations. The
differences in the way edges are handled are a quirk of the current implementation and dynarmic will likely add a
function analyser in the medium-term future.

Dynarmic's intermediate representation is typed. Each microinstruction may take zero or more arguments and may
return zero or more arguments. A subset of the microinstructions available is documented below.

A complete list of microinstructions can be found in [src/dynarmic/ir/opcodes.inc](../src/dynarmic/ir/opcodes.inc).

The below lists some commonly used microinstructions.

### Immediate: Imm{U1,U8,U32,RegRef}

    <u1> ImmU1(u1 value)
    <u8> ImmU8(u8 value)
    <u32> ImmU32(u32 value)
    <RegRef> ImmRegRef(Arm::Reg gpr)

These instructions take a `bool`, `u8` or `u32` value and wraps it up in an IR node so that they can be used
by the IR.

### Context: {Get,Set}Register

    <u32> GetRegister(<RegRef> reg)
    <void> SetRegister(<RegRef> reg, <u32> value)

Gets and sets `JitState::Reg[reg]`. Note that `SetRegister(Arm::Reg::R15, _)` is disallowed by IRBuilder.
Use `{ALU,BX}WritePC` instead.

Note that sequences like `SetRegister(R4, _)` followed by `GetRegister(R4)` are
optimized away.

### Context: {Get,Set}{N,Z,C,V}Flag

    <u1> GetNFlag()
    <void> SetNFlag(<u1> value)
    <u1> GetZFlag()
    <void> SetZFlag(<u1> value)
    <u1> GetCFlag()
    <void> SetCFlag(<u1> value)
    <u1> GetVFlag()
    <void> SetVFlag(<u1> value)

Gets and sets bits in `JitState::Cpsr`. Similarly to registers redundant get/sets are optimized away.

### Context: BXWritePC

    <void> BXWritePC(<u32> value)

This should probably be the last instruction in a translation block unless you're doing something fancy.

This microinstruction sets R15 and CPSR.T as appropriate.

### Callback: CallSupervisor

    <void> CallSupervisor(<u32> svc_imm32)

This should probably be the last instruction in a translation block unless you're doing something fancy.

### Calculation: LastSignificant{Half,Byte}

    <u16> LeastSignificantHalf(<u32> value)
    <u8> LeastSignificantByte(<u32> value)

Extract a u16 and u8 respectively from a u32.

### Calculation: MostSignificantBit, IsZero

    <u1> MostSignificantBit(<u32> value)
    <u1> IsZero(<u32> value)

These are used to implement ARM flags N and Z. These can often be optimized away by the backend into a host flag read.

### Calculation: LogicalShiftLeft

    (<u32> result, <u1> carry_out) LogicalShiftLeft(<u32> operand, <u8> shift_amount, <u1> carry_in)

Pseudocode:

        if shift_amount == 0:
            return (operand, carry_in)

        x = operand * (2 ** shift_amount)
        result = Bits<31,0>(x)
        carry_out = Bit<32>(x)

        return (result, carry_out)

This follows ARM semantics. Note `shift_amount` is not masked to 5 bits (like `SHL` does on x64).

### Calculation: LogicalShiftRight

    (<u32> result, <u1> carry_out) LogicalShiftLeft(<u32> operand, <u8> shift_amount, <u1> carry_in)

Pseudocode:

        if shift_amount == 0:
            return (operand, carry_in)

        x = ZeroExtend(operand, from_size: 32, to_size: shift_amount+32)
        result = Bits<shift_amount+31,shift_amount>(x)
        carry_out = Bit<shift_amount-1>(x)

        return (result, carry_out)

This follows ARM semantics. Note `shift_amount` is not masked to 5 bits (like `SHR` does on x64).

### Calculation: ArithmeticShiftRight

    (<u32> result, <u1> carry_out) ArithmeticShiftRight(<u32> operand, <u8> shift_amount, <u1> carry_in)

Pseudocode:

        if shift_amount == 0:
            return (operand, carry_in)

        x = SignExtend(operand, from_size: 32, to_size: shift_amount+32)
        result = Bits<shift_amount+31,shift_amount>(x)
        carry_out = Bit<shift_amount-1>(x)

        return (result, carry_out)

This follows ARM semantics. Note `shift_amount` is not masked to 5 bits (like `SAR` does on x64).

### Calcuation: RotateRight

    (<u32> result, <u1> carry_out) RotateRight(<u32> operand, <u8> shift_amount, <u1> carry_in)

Pseudocode:

        if shift_amount == 0:
            return (operand, carry_in)

        shift_amount %= 32
        result = (operand << shift_amount) | (operand >> (32 - shift_amount))
        carry_out = Bit<31>(result)

        return (result, carry_out)

### Calculation: AddWithCarry

    (<u32> result, <u1> carry_out, <u1> overflow) AddWithCarry(<u32> a, <u32> b, <u1> carry_in)

a + b + carry_in

### Calculation: SubWithCarry

    (<u32> result, <u1> carry_out, <u1> overflow) SubWithCarry(<u32> a, <u32> b, <u1> carry_in)

This has equivalent semantics to `AddWithCarry(a, Not(b), carry_in)`.

a - b - !carry_in

### Calculation: And

    <u32> And(<u32> a, <u32> b)

### Calculation: Eor

    <u32> Eor(<u32> a, <u32> b)

Exclusive OR (i.e.: XOR)

### Calculation: Or

    <u32> Or(<u32> a, <u32> b)

### Calculation: Not

    <u32> Not(<u32> value)

### Callback: {Read,Write}Memory{8,16,32,64}

```c++
<u8> ReadMemory8(<u32> vaddr)
<u8> ReadMemory16(<u32> vaddr)
<u8> ReadMemory32(<u32> vaddr)
<u8> ReadMemory64(<u32> vaddr)
<void> WriteMemory8(<u32> vaddr, <u8> value_to_store)
<void> WriteMemory16(<u32> vaddr, <u16> value_to_store)
<void> WriteMemory32(<u32> vaddr, <u32> value_to_store)
<void> WriteMemory64(<u32> vaddr, <u64> value_to_store)
```

Memory access.

### Terminal: Interpret

```c++
SetTerm(IR::Term::Interpret{next})
```

This terminal instruction calls the interpreter, starting at `next`.
The interpreter must interpret exactly one instruction.

### Terminal: ReturnToDispatch

```c++
SetTerm(IR::Term::ReturnToDispatch{})
```

This terminal instruction returns control to the dispatcher.
The dispatcher will use the value in R15 to determine what comes next.

### Terminal: LinkBlock

```c++
SetTerm(IR::Term::LinkBlock{next})
```

This terminal instruction jumps to the basic block described by `next` if we have enough
cycles remaining. If we do not have enough cycles remaining, we return to the
dispatcher, which will return control to the host.

### Terminal: LinkBlockFast

```c++
SetTerm(IR::Term::LinkBlockFast{next})
```

This terminal instruction jumps to the basic block described by `next` unconditionally.
This promises guarantees that must be held at runtime - i.e that the program wont hang,

### Terminal: PopRSBHint

```c++
SetTerm(IR::Term::PopRSBHint{})
```

This terminal instruction checks the top of the Return Stack Buffer against R15.
If RSB lookup fails, control is returned to the dispatcher.
This is an optimization for faster function calls. A backend that doesn't support
this optimization or doesn't have a RSB may choose to implement this exactly as
`ReturnToDispatch`.

### Terminal: If

```c++
SetTerm(IR::Term::If{cond, term_then, term_else})
```

This terminal instruction conditionally executes one terminal or another depending
on the run-time state of the ARM flags.

# Register Allocation (x64 Backend)

`HostLoc`s contain values. A `HostLoc` ("host value location") is either a host CPU register or a host spill location.

Values once set cannot be changed. Values can however be moved by the register allocator between `HostLoc`s. This is
handled by the register allocator itself and code that uses the register allocator need not and should not move values
between registers.

The register allocator is based on three concepts: `Use`, `Def` and `Scratch`.

* `Use`: The use of a value.
* `Define`: The definition of a value, this is the only time when a value is set.
* `Scratch`: Allocate a register that can be freely modified as one wishes.

Note that `Use`ing a value decrements its `use_count` by one. When the `use_count` reaches zero the value is discarded and no longer exists.

The member functions on `RegAlloc` are just a combination of the above concepts.

The following registers are reserved for internal use and should NOT participate in register allocation:
- `%xmm0`, `%xmm1`, `%xmm2`: Used as scratch in exclusive memory access.
- `%rsp`: Stack pointer.
- `%r15`: JIT pointer
- `%r14`: Page table pointer.
- `%r13`: Fastmem pointer.

The layout convenes `%r15` as the JIT state pointer - while it may be tempting to turn it into a synthetic pointer, keeping an entire register (out of 12 available) is preferable over inlining a directly computed immediate.

Do NEVER modify `%r15`, we must make it clear that this register is "immutable" for the entirety of the JIT block duration.

### `Scratch`

```c++
Xbyak::Reg64 ScratchGpr(HostLocList desired_locations = any_gpr);
Xbyak::Xmm ScratchXmm(HostLocList desired_locations = any_xmm);
```

At runtime, allocate one of the registers in `desired_locations`. You are free to modify the register. The register is discarded at the end of the allocation scope.

### Pure `Use`

```c++
Xbyak::Reg64 UseGpr(Argument& arg);
Xbyak::Xmm UseXmm(Argument& arg);
OpArg UseOpArg(Argument& arg);
void Use(Argument& arg, HostLoc host_loc);
```

At runtime, the value corresponding to `arg` will be placed a register. The actual register is determined by
which one of the above functions is called. `UseGpr` places it in an unused GPR, `UseXmm` places it
in an unused XMM register, `UseOpArg` might be in a register or might be a memory location, and `Use` allows
you to specify a specific register (GPR or XMM) to use.

This register **must not** have it's value changed.

### `UseScratch`

```c++
Xbyak::Reg64 UseScratchGpr(Argument& arg);
Xbyak::Xmm UseScratchXmm(Argument& arg);
void UseScratch(Argument& arg, HostLoc host_loc);
```

At runtime, the value corresponding to `arg` will be placed a register. The actual register is determined by
which one of the above functions is called. `UseScratchGpr` places it in an unused GPR, `UseScratchXmm` places it
in an unused XMM register, and `UseScratch` allows you to specify a specific register (GPR or XMM) to use.

The return value is the register allocated to you.

You are free to modify the value in the register. The register is discarded at the end of the allocation scope.

### `Define` as register

A `Define` is the defintion of a value. This is the only time when a value may be set.

```c++
void DefineValue(IR::Inst* inst, const Xbyak::Reg& reg);
```

By calling `DefineValue`, you are stating that you wish to define the value for `inst`, and you have written the
value to the specified register `reg`.

### `Define`ing as an alias of a different value

Adding a `Define` to an existing value.

```c++
void DefineValue(IR::Inst* inst, Argument& arg);
```

You are declaring that the value for `inst` is the same as the value for `arg`. No host machine instructions are
emitted.

## When to use each?

* Prefer `Use` to `UseScratch` where possible.
* Prefer the `OpArg` variants where possible.
* Prefer to **not** use the specific `HostLoc` variants where possible.

# Return Stack Buffer Optimization (x64 Backend)

One of the optimizations that dynarmic does is block-linking. Block-linking is done when
the destination address of a jump is available at JIT-time. Instead of returning to the
dispatcher at the end of a block we can perform block-linking: just jump directly to the
next block. This is beneficial because returning to the dispatcher can often be quite
expensive.

What should we do in cases when we can't predict the destination address? The eponymous
example is when executing a return statement at the end of a function; the return address
is not statically known at compile time.

We deal with this by using a return stack buffer: When we execute a call instruction,
we push our prediction onto the RSB. When we execute a return instruction, we pop a
prediction off the RSB. If the prediction is a hit, we immediately jump to the relevant
compiled block. Otherwise, we return to the dispatcher.

This is the essential idea behind this optimization.

## `UniqueHash`

One complication dynarmic has is that a compiled block is not uniquely identifiable by
the PC alone, but bits in the FPSCR and CPSR are also relevant. We resolve this by
computing a 64-bit `UniqueHash` that is guaranteed to uniquely identify a block.

```c++
u64 LocationDescriptor::UniqueHash() const {
    // This value MUST BE UNIQUE.
    // This calculation has to match up with EmitX64::EmitTerminalPopRSBHint
    u64 pc_u64 = u64(arm_pc) << 32;
    u64 fpscr_u64 = u64(fpscr.Value());
    u64 t_u64 = cpsr.T() ? 1 : 0;
    u64 e_u64 = cpsr.E() ? 2 : 0;
    return pc_u64 | fpscr_u64 | t_u64 | e_u64;
}
```

## Our implementation isn't actually a stack

Dynarmic's RSB isn't actually a stack. It was implemented as a ring buffer because
that showed better performance in tests.

### RSB Structure

The RSB is implemented as a ring buffer. `rsb_ptr` is the index of the insertion
point. Each element in `rsb_location_descriptors` is a `UniqueHash` and they
each correspond to an element in `rsb_codeptrs`. `rsb_codeptrs` contains the
host addresses for the corresponding the compiled blocks.

`RSBSize` was chosen by performance testing. Note that this is bigger than the
size of the real RSB in hardware (which has 3 entries). Larger RSBs than 8
showed degraded performance.

```c++
struct JitState {
    // ...

    static constexpr size_t RSBSize = 8; // MUST be a power of 2.
    u32 rsb_ptr = 0;
    std::array<u64, RSBSize> rsb_location_descriptors;
    std::array<u64, RSBSize> rsb_codeptrs;
    void ResetRSB();

    // ...
};
```

### RSB Push

We insert our prediction at the insertion point iff the RSB doesn't already
contain a prediction with the same `UniqueHash`.

```c++
void EmitX64::EmitPushRSB(IR::Block&, IR::Inst* inst) {
    using namespace Xbyak::util;

    ASSERT(inst->GetArg(0).IsImmediate());
    u64 imm64 = inst->GetArg(0).GetU64();

    Xbyak::Reg64 code_ptr_reg = reg_alloc.ScratchGpr(code, {HostLoc::RCX});
    Xbyak::Reg64 loc_desc_reg = reg_alloc.ScratchGpr(code);
    Xbyak::Reg32 index_reg = reg_alloc.ScratchGpr(code).cvt32();
    u64 code_ptr = unique_hash_to_code_ptr.find(imm64) != unique_hash_to_code_ptr.end()
                    ? u64(unique_hash_to_code_ptr[imm64])
                    : u64(code->GetReturnFromRunCodeAddress());

    code->mov(index_reg, dword[code.ABI_JIT_PTR + offsetof(JitState, rsb_ptr)]);
    code->add(index_reg, 1);
    code->and_(index_reg, u32(JitState::RSBSize - 1));

    code->mov(loc_desc_reg, u64(imm64));
    CodePtr patch_location = code->getCurr<CodePtr>();
    patch_unique_hash_locations[imm64].emplace_back(patch_location);
    code->mov(code_ptr_reg, u64(code_ptr)); // This line has to match up with EmitX64::Patch.
    code->EnsurePatchLocationSize(patch_location, 10);

    Xbyak::Label label;
    for (size_t i = 0; i < JitState::RSBSize; ++i) {
        code->cmp(loc_desc_reg, qword[code.ABI_JIT_PTR + offsetof(JitState, rsb_location_descriptors) + i * sizeof(u64)]);
        code->je(label, code->T_SHORT);
    }

    code->mov(dword[code.ABI_JIT_PTR + offsetof(JitState, rsb_ptr)], index_reg);
    code->mov(qword[code.ABI_JIT_PTR + index_reg.cvt64() * 8 + offsetof(JitState, rsb_location_descriptors)], loc_desc_reg);
    code->mov(qword[code.ABI_JIT_PTR + index_reg.cvt64() * 8 + offsetof(JitState, rsb_codeptrs)], code_ptr_reg);
    code->L(label);
}
```

In pseudocode:

```c++
    for (i := 0 .. RSBSize-1)
        if (rsb_location_descriptors[i] == imm64)
        goto label;
    rsb_ptr++;
    rsb_ptr %= RSBSize;
    rsb_location_desciptors[rsb_ptr] = imm64; //< The UniqueHash
    rsb_codeptr[rsb_ptr] = /* codeptr corresponding to the UniqueHash */;
label:
```

## RSB Pop

To check if a predicition is in the RSB, we linearly scan the RSB.

```c++
void EmitX64::EmitTerminalPopRSBHint(IR::Term::PopRSBHint, IR::LocationDescriptor initial_location) {
    using namespace Xbyak::util;

    // This calculation has to match up with IREmitter::PushRSB
    code->mov(ecx, MJitStateReg(Arm::Reg::PC));
    code->shl(rcx, 32);
    code->mov(ebx, dword[code.ABI_JIT_PTR + offsetof(JitState, FPSCR_mode)]);
    code->or_(ebx, dword[code.ABI_JIT_PTR + offsetof(JitState, CPSR_et)]);
    code->or_(rbx, rcx);

    code->mov(rax, u64(code->GetReturnFromRunCodeAddress()));
    for (size_t i = 0; i < JitState::RSBSize; ++i) {
        code->cmp(rbx, qword[code.ABI_JIT_PTR + offsetof(JitState, rsb_location_descriptors) + i * sizeof(u64)]);
        code->cmove(rax, qword[code.ABI_JIT_PTR + offsetof(JitState, rsb_codeptrs) + i * sizeof(u64)]);
    }

    code->jmp(rax);
}
```

In pseudocode:

```c++
rbx := ComputeUniqueHash()
rax := ReturnToDispatch
for (i := 0 .. RSBSize-1)
    if (rbx == rsb_location_descriptors[i])
        rax = rsb_codeptrs[i]
goto rax
```

# Fast memory (Fastmem)

The main way of accessing memory in JITed programs is via an invoked function, say "Read()" and "Write()". On our translator, such functions usually take a sizable amounts of code space (push + call + pop). Trash the i-cache (due to an indirect call) and overall make code emission more bloated.

The solution? Delegate invalid accesses to a dedicated arena, similar to a swap. The main idea behind such mechanism is to allow the OS to transmit page faults from invalid accesses into the JIT translator directly, bypassing address space calls, while this sacrifices i-cache coherency, it allows for smaller code-size and "faster" throguhput.

Many kernels however, do not support fast signal dispatching (Solaris, OpenBSD, FreeBSD). Only Linux and Windows support relatively "fast" signal dispatching. Hence this feature is better suited for them only.

![Host to guest translation](./HostToGuest.svg)

![Fastmem translation](./Fastmem.svg)

In x86_64 for example, when a page fault occurs, the CPU will transmit via control registers and the stack (see `IRETQ`) the appropriate arguments for a page fault handler, the OS then will transform that into something that can be sent into userspace.

Most modern OSes implement kernel-page-table-isolation, which means a set of system calls will invoke a context switch (not often used syscalls), whereas others are handled by the same process address space (the smaller kernel portion, often used syscalls) without needing a context switch. This effect can be negated on systems with PCID (up to 4096 unique IDs).

Signal dispatching takes a performance hit from reloading `%cr3` - but Linux does something more clever to avoid reloads:  VDSO will take care of the entire thing in the same address space. Making dispatching as costly as an indirect call - without the hazards of increased code size.

The main downside from this is the constant i-cache trashing and pipeline hazards introduced by the VDSO signal handlers. However on most benchmarks fastmem does perform faster than without (Linux only). This also abuses the fact of continous address space emulation by using an arena - which can then be potentially transparently mapped into a hugepage, reducing TLB walk times.
