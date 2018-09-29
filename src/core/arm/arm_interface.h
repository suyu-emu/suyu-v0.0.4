// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/common_types.h"

namespace Kernel {
enum class VMAPermission : u8;
}

namespace Core {

/// Generic ARMv8 CPU interface
class ARM_Interface : NonCopyable {
public:
    virtual ~ARM_Interface() {}

    struct ThreadContext {
        std::array<u64, 31> cpu_registers;
        u64 sp;
        u64 pc;
        u32 pstate;
        std::array<u8, 4> padding;
        std::array<u128, 32> vector_registers;
        u32 fpcr;
        u32 fpsr;
        u64 tpidr;
    };
    // Internally within the kernel, it expects the AArch64 version of the
    // thread context to be 800 bytes in size.
    static_assert(sizeof(ThreadContext) == 0x320);

    /// Runs the CPU until an event happens
    virtual void Run() = 0;

    /// Step CPU by one instruction
    virtual void Step() = 0;

    /// Maps a backing memory region for the CPU
    virtual void MapBackingMemory(VAddr address, std::size_t size, u8* memory,
                                  Kernel::VMAPermission perms) = 0;

    /// Unmaps a region of memory that was previously mapped using MapBackingMemory
    virtual void UnmapMemory(VAddr address, std::size_t size) = 0;

    /// Clear all instruction cache
    virtual void ClearInstructionCache() = 0;

    /// Notify CPU emulation that page tables have changed
    virtual void PageTableChanged() = 0;

    /**
     * Set the Program Counter to an address
     * @param addr Address to set PC to
     */
    virtual void SetPC(u64 addr) = 0;

    /*
     * Get the current Program Counter
     * @return Returns current PC
     */
    virtual u64 GetPC() const = 0;

    /**
     * Get an ARM register
     * @param index Register index
     * @return Returns the value in the register
     */
    virtual u64 GetReg(int index) const = 0;

    /**
     * Set an ARM register
     * @param index Register index
     * @param value Value to set register to
     */
    virtual void SetReg(int index, u64 value) = 0;

    /**
     * Gets the value of a specified vector register.
     *
     * @param index The index of the vector register.
     * @return the value within the vector register.
     */
    virtual u128 GetVectorReg(int index) const = 0;

    /**
     * Sets a given value into a vector register.
     *
     * @param index The index of the vector register.
     * @param value The new value to place in the register.
     */
    virtual void SetVectorReg(int index, u128 value) = 0;

    /**
     * Get the current PSTATE register
     * @return Returns the value of the PSTATE register
     */
    virtual u32 GetPSTATE() const = 0;

    /**
     * Set the current PSTATE register
     * @param pstate Value to set PSTATE to
     */
    virtual void SetPSTATE(u32 pstate) = 0;

    virtual VAddr GetTlsAddress() const = 0;

    virtual void SetTlsAddress(VAddr address) = 0;

    /**
     * Gets the value within the TPIDR_EL0 (read/write software thread ID) register.
     *
     * @return the value within the register.
     */
    virtual u64 GetTPIDR_EL0() const = 0;

    /**
     * Sets a new value within the TPIDR_EL0 (read/write software thread ID) register.
     *
     * @param value The new value to place in the register.
     */
    virtual void SetTPIDR_EL0(u64 value) = 0;

    /**
     * Saves the current CPU context
     * @param ctx Thread context to save
     */
    virtual void SaveContext(ThreadContext& ctx) = 0;

    /**
     * Loads a CPU context
     * @param ctx Thread context to load
     */
    virtual void LoadContext(const ThreadContext& ctx) = 0;

    /// Clears the exclusive monitor's state.
    virtual void ClearExclusiveState() = 0;

    /// Prepare core for thread reschedule (if needed to correctly handle state)
    virtual void PrepareReschedule() = 0;
};

} // namespace Core
