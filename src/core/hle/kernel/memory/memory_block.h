// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "core/hle/kernel/memory/memory_types.h"
#include "core/hle/kernel/svc_types.h"

namespace Kernel::Memory {

enum class MemoryState : u32 {
    None = 0,
    Mask = 0xFF,
    All = ~None,

    FlagCanReprotect = (1 << 8),
    FlagCanDebug = (1 << 9),
    FlagCanUseIpc = (1 << 10),
    FlagCanUseNonDeviceIpc = (1 << 11),
    FlagCanUseNonSecureIpc = (1 << 12),
    FlagMapped = (1 << 13),
    FlagCode = (1 << 14),
    FlagCanAlias = (1 << 15),
    FlagCanCodeAlias = (1 << 16),
    FlagCanTransfer = (1 << 17),
    FlagCanQueryPhysical = (1 << 18),
    FlagCanDeviceMap = (1 << 19),
    FlagCanAlignedDeviceMap = (1 << 20),
    FlagCanIpcUserBuffer = (1 << 21),
    FlagReferenceCounted = (1 << 22),
    FlagCanMapProcess = (1 << 23),
    FlagCanChangeAttribute = (1 << 24),
    FlagCanCodeMemory = (1 << 25),

    FlagsData = FlagCanReprotect | FlagCanUseIpc | FlagCanUseNonDeviceIpc | FlagCanUseNonSecureIpc |
                FlagMapped | FlagCanAlias | FlagCanTransfer | FlagCanQueryPhysical |
                FlagCanDeviceMap | FlagCanAlignedDeviceMap | FlagCanIpcUserBuffer |
                FlagReferenceCounted | FlagCanChangeAttribute,

    FlagsCode = FlagCanDebug | FlagCanUseIpc | FlagCanUseNonDeviceIpc | FlagCanUseNonSecureIpc |
                FlagMapped | FlagCode | FlagCanQueryPhysical | FlagCanDeviceMap |
                FlagCanAlignedDeviceMap | FlagReferenceCounted,

    FlagsMisc = FlagMapped | FlagReferenceCounted | FlagCanQueryPhysical | FlagCanDeviceMap,

    Free = static_cast<u32>(Svc::MemoryState::Free),
    Io = static_cast<u32>(Svc::MemoryState::Io) | FlagMapped,
    Static = static_cast<u32>(Svc::MemoryState::Static) | FlagMapped | FlagCanQueryPhysical,
    Code = static_cast<u32>(Svc::MemoryState::Code) | FlagsCode | FlagCanMapProcess,
    CodeData = static_cast<u32>(Svc::MemoryState::CodeData) | FlagsData | FlagCanMapProcess |
               FlagCanCodeMemory,
    Shared = static_cast<u32>(Svc::MemoryState::Shared) | FlagMapped | FlagReferenceCounted,
    Normal = static_cast<u32>(Svc::MemoryState::Normal) | FlagsData | FlagCanCodeMemory,

    AliasCode = static_cast<u32>(Svc::MemoryState::AliasCode) | FlagsCode | FlagCanMapProcess |
                FlagCanCodeAlias,
    AliasCodeData = static_cast<u32>(Svc::MemoryState::AliasCodeData) | FlagsData |
                    FlagCanMapProcess | FlagCanCodeAlias | FlagCanCodeMemory,

    Ipc = static_cast<u32>(Svc::MemoryState::Ipc) | FlagsMisc | FlagCanAlignedDeviceMap |
          FlagCanUseIpc | FlagCanUseNonSecureIpc | FlagCanUseNonDeviceIpc,

    Stack = static_cast<u32>(Svc::MemoryState::Stack) | FlagsMisc | FlagCanAlignedDeviceMap |
            FlagCanUseIpc | FlagCanUseNonSecureIpc | FlagCanUseNonDeviceIpc,

    ThreadLocal =
        static_cast<u32>(Svc::MemoryState::ThreadLocal) | FlagMapped | FlagReferenceCounted,

    Transferred = static_cast<u32>(Svc::MemoryState::Transferred) | FlagsMisc |
                  FlagCanAlignedDeviceMap | FlagCanChangeAttribute | FlagCanUseIpc |
                  FlagCanUseNonSecureIpc | FlagCanUseNonDeviceIpc,

    SharedTransferred = static_cast<u32>(Svc::MemoryState::SharedTransferred) | FlagsMisc |
                        FlagCanAlignedDeviceMap | FlagCanUseNonSecureIpc | FlagCanUseNonDeviceIpc,

    SharedCode = static_cast<u32>(Svc::MemoryState::SharedCode) | FlagMapped |
                 FlagReferenceCounted | FlagCanUseNonSecureIpc | FlagCanUseNonDeviceIpc,

    Inaccessible = static_cast<u32>(Svc::MemoryState::Inaccessible),

    NonSecureIpc = static_cast<u32>(Svc::MemoryState::NonSecureIpc) | FlagsMisc |
                   FlagCanAlignedDeviceMap | FlagCanUseNonSecureIpc | FlagCanUseNonDeviceIpc,

    NonDeviceIpc =
        static_cast<u32>(Svc::MemoryState::NonDeviceIpc) | FlagsMisc | FlagCanUseNonDeviceIpc,

    Kernel = static_cast<u32>(Svc::MemoryState::Kernel) | FlagMapped,

    GeneratedCode = static_cast<u32>(Svc::MemoryState::GeneratedCode) | FlagMapped |
                    FlagReferenceCounted | FlagCanDebug,
    CodeOut = static_cast<u32>(Svc::MemoryState::CodeOut) | FlagMapped | FlagReferenceCounted,
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryState);

static_assert(static_cast<u32>(MemoryState::Free) == 0x00000000);
static_assert(static_cast<u32>(MemoryState::Io) == 0x00002001);
static_assert(static_cast<u32>(MemoryState::Static) == 0x00042002);
static_assert(static_cast<u32>(MemoryState::Code) == 0x00DC7E03);
static_assert(static_cast<u32>(MemoryState::CodeData) == 0x03FEBD04);
static_assert(static_cast<u32>(MemoryState::Normal) == 0x037EBD05);
static_assert(static_cast<u32>(MemoryState::Shared) == 0x00402006);
static_assert(static_cast<u32>(MemoryState::AliasCode) == 0x00DD7E08);
static_assert(static_cast<u32>(MemoryState::AliasCodeData) == 0x03FFBD09);
static_assert(static_cast<u32>(MemoryState::Ipc) == 0x005C3C0A);
static_assert(static_cast<u32>(MemoryState::Stack) == 0x005C3C0B);
static_assert(static_cast<u32>(MemoryState::ThreadLocal) == 0x0040200C);
static_assert(static_cast<u32>(MemoryState::Transferred) == 0x015C3C0D);
static_assert(static_cast<u32>(MemoryState::SharedTransferred) == 0x005C380E);
static_assert(static_cast<u32>(MemoryState::SharedCode) == 0x0040380F);
static_assert(static_cast<u32>(MemoryState::Inaccessible) == 0x00000010);
static_assert(static_cast<u32>(MemoryState::NonSecureIpc) == 0x005C3811);
static_assert(static_cast<u32>(MemoryState::NonDeviceIpc) == 0x004C2812);
static_assert(static_cast<u32>(MemoryState::Kernel) == 0x00002013);
static_assert(static_cast<u32>(MemoryState::GeneratedCode) == 0x00402214);
static_assert(static_cast<u32>(MemoryState::CodeOut) == 0x00402015);

enum class MemoryPermission : u8 {
    None = 0,
    Mask = static_cast<u8>(~None),

    Read = 1 << 0,
    Write = 1 << 1,
    Execute = 1 << 2,

    ReadAndWrite = Read | Write,
    ReadAndExecute = Read | Execute,

    UserMask = static_cast<u8>(Svc::MemoryPermission::Read | Svc::MemoryPermission::Write |
                               Svc::MemoryPermission::Execute),
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryPermission);

enum class MemoryAttribute : u8 {
    None = 0x00,
    Mask = 0x7F,
    All = Mask,
    DontCareMask = 0x80,

    Locked = static_cast<u8>(Svc::MemoryAttribute::Locked),
    IpcLocked = static_cast<u8>(Svc::MemoryAttribute::IpcLocked),
    DeviceShared = static_cast<u8>(Svc::MemoryAttribute::DeviceShared),
    Uncached = static_cast<u8>(Svc::MemoryAttribute::Uncached),

    IpcAndDeviceMapped = IpcLocked | DeviceShared,
    LockedAndIpcLocked = Locked | IpcLocked,
    DeviceSharedAndUncached = DeviceShared | Uncached
};
DECLARE_ENUM_FLAG_OPERATORS(MemoryAttribute);

static_assert((static_cast<u8>(MemoryAttribute::Mask) &
               static_cast<u8>(MemoryAttribute::DontCareMask)) == 0);

struct MemoryInfo {
    VAddr addr{};
    std::size_t size{};
    MemoryState state{};
    MemoryPermission perm{};
    MemoryAttribute attribute{};
    MemoryPermission original_perm{};
    u16 ipc_lock_count{};
    u16 device_use_count{};

    constexpr Svc::MemoryInfo GetSvcMemoryInfo() const {
        return {
            addr,
            size,
            static_cast<Svc::MemoryState>(state & MemoryState::Mask),
            static_cast<Svc::MemoryAttribute>(attribute & MemoryAttribute::Mask),
            static_cast<Svc::MemoryPermission>(perm & MemoryPermission::UserMask),
            ipc_lock_count,
            device_use_count,
        };
    }

    constexpr VAddr GetAddress() const {
        return addr;
    }
    constexpr std::size_t GetSize() const {
        return size;
    }
    constexpr std::size_t GetNumPages() const {
        return GetSize() / PageSize;
    }
    constexpr VAddr GetEndAddress() const {
        return GetAddress() + GetSize();
    }
    constexpr VAddr GetLastAddress() const {
        return GetEndAddress() - 1;
    }
};

class MemoryBlock final {
    friend class MemoryBlockManager;

private:
    VAddr addr{};
    std::size_t num_pages{};
    MemoryState state{MemoryState::None};
    u16 ipc_lock_count{};
    u16 device_use_count{};
    MemoryPermission perm{MemoryPermission::None};
    MemoryPermission original_perm{MemoryPermission::None};
    MemoryAttribute attribute{MemoryAttribute::None};

public:
    static constexpr int Compare(const MemoryBlock& lhs, const MemoryBlock& rhs) {
        if (lhs.GetAddress() < rhs.GetAddress()) {
            return -1;
        } else if (lhs.GetAddress() <= rhs.GetLastAddress()) {
            return 0;
        } else {
            return 1;
        }
    }

public:
    constexpr MemoryBlock() = default;
    constexpr MemoryBlock(VAddr addr_, std::size_t num_pages_, MemoryState state_,
                          MemoryPermission perm_, MemoryAttribute attribute_)
        : addr{addr_}, num_pages(num_pages_), state{state_}, perm{perm_}, attribute{attribute_} {}

    constexpr VAddr GetAddress() const {
        return addr;
    }

    constexpr std::size_t GetNumPages() const {
        return num_pages;
    }

    constexpr std::size_t GetSize() const {
        return GetNumPages() * PageSize;
    }

    constexpr VAddr GetEndAddress() const {
        return GetAddress() + GetSize();
    }

    constexpr VAddr GetLastAddress() const {
        return GetEndAddress() - 1;
    }

    constexpr MemoryInfo GetMemoryInfo() const {
        return {
            GetAddress(), GetSize(),     state,          perm,
            attribute,    original_perm, ipc_lock_count, device_use_count,
        };
    }

    void ShareToDevice(MemoryPermission /*new_perm*/) {
        ASSERT((attribute & MemoryAttribute::DeviceShared) == MemoryAttribute::DeviceShared ||
               device_use_count == 0);
        attribute |= MemoryAttribute::DeviceShared;
        const u16 new_use_count{++device_use_count};
        ASSERT(new_use_count > 0);
    }

    void UnshareToDevice(MemoryPermission /*new_perm*/) {
        ASSERT((attribute & MemoryAttribute::DeviceShared) == MemoryAttribute::DeviceShared);
        const u16 prev_use_count{device_use_count--};
        ASSERT(prev_use_count > 0);
        if (prev_use_count == 1) {
            attribute &= ~MemoryAttribute::DeviceShared;
        }
    }

private:
    constexpr bool HasProperties(MemoryState s, MemoryPermission p, MemoryAttribute a) const {
        constexpr MemoryAttribute AttributeIgnoreMask{MemoryAttribute::DontCareMask |
                                                      MemoryAttribute::IpcLocked |
                                                      MemoryAttribute::DeviceShared};
        return state == s && perm == p &&
               (attribute | AttributeIgnoreMask) == (a | AttributeIgnoreMask);
    }

    constexpr bool HasSameProperties(const MemoryBlock& rhs) const {
        return state == rhs.state && perm == rhs.perm && original_perm == rhs.original_perm &&
               attribute == rhs.attribute && ipc_lock_count == rhs.ipc_lock_count &&
               device_use_count == rhs.device_use_count;
    }

    constexpr bool Contains(VAddr start) const {
        return GetAddress() <= start && start <= GetEndAddress();
    }

    constexpr void Add(std::size_t count) {
        ASSERT(count > 0);
        ASSERT(GetAddress() + count * PageSize - 1 < GetEndAddress() + count * PageSize - 1);

        num_pages += count;
    }

    constexpr void Update(MemoryState new_state, MemoryPermission new_perm,
                          MemoryAttribute new_attribute) {
        ASSERT(original_perm == MemoryPermission::None);
        ASSERT((attribute & MemoryAttribute::IpcLocked) == MemoryAttribute::None);

        state = new_state;
        perm = new_perm;

        attribute = static_cast<MemoryAttribute>(
            new_attribute |
            (attribute & (MemoryAttribute::IpcLocked | MemoryAttribute::DeviceShared)));
    }

    constexpr MemoryBlock Split(VAddr split_addr) {
        ASSERT(GetAddress() < split_addr);
        ASSERT(Contains(split_addr));
        ASSERT(Common::IsAligned(split_addr, PageSize));

        MemoryBlock block;
        block.addr = addr;
        block.num_pages = (split_addr - GetAddress()) / PageSize;
        block.state = state;
        block.ipc_lock_count = ipc_lock_count;
        block.device_use_count = device_use_count;
        block.perm = perm;
        block.original_perm = original_perm;
        block.attribute = attribute;

        addr = split_addr;
        num_pages -= block.num_pages;

        return block;
    }
};
static_assert(std::is_trivially_destructible<MemoryBlock>::value);

} // namespace Kernel::Memory
