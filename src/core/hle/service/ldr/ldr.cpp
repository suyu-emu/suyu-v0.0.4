// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <fmt/format.h>

#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/ldr/ldr.h"
#include "core/hle/service/service.h"
#include "core/loader/nro.h"

namespace Service::LDR {

namespace ErrCodes {
enum {
    InvalidNRO = 52,
    InvalidNRR = 53,
    MissingNRRHash = 54,
    MaximumNRO = 55,
    MaximumNRR = 56,
    AlreadyLoaded = 57,
    InvalidAlignment = 81,
    InvalidSize = 82,
    InvalidNROAddress = 84,
    InvalidNRRAddress = 85,
    NotInitialized = 87,
};
}

constexpr ResultCode ERROR_INVALID_NRO(ErrorModule::Loader, ErrCodes::InvalidNRO);
constexpr ResultCode ERROR_INVALID_NRR(ErrorModule::Loader, ErrCodes::InvalidNRR);
constexpr ResultCode ERROR_MISSING_NRR_HASH(ErrorModule::Loader, ErrCodes::MissingNRRHash);
constexpr ResultCode ERROR_MAXIMUM_NRO(ErrorModule::Loader, ErrCodes::MaximumNRO);
constexpr ResultCode ERROR_MAXIMUM_NRR(ErrorModule::Loader, ErrCodes::MaximumNRR);
constexpr ResultCode ERROR_ALREADY_LOADED(ErrorModule::Loader, ErrCodes::AlreadyLoaded);
constexpr ResultCode ERROR_INVALID_ALIGNMENT(ErrorModule::Loader, ErrCodes::InvalidAlignment);
constexpr ResultCode ERROR_INVALID_SIZE(ErrorModule::Loader, ErrCodes::InvalidSize);
constexpr ResultCode ERROR_INVALID_NRO_ADDRESS(ErrorModule::Loader, ErrCodes::InvalidNROAddress);
constexpr ResultCode ERROR_INVALID_NRR_ADDRESS(ErrorModule::Loader, ErrCodes::InvalidNRRAddress);
constexpr ResultCode ERROR_NOT_INITIALIZED(ErrorModule::Loader, ErrCodes::NotInitialized);

constexpr u64 MAXIMUM_LOADED_RO = 0x40;

class DebugMonitor final : public ServiceFramework<DebugMonitor> {
public:
    explicit DebugMonitor() : ServiceFramework{"ldr:dmnt"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "AddProcessToDebugLaunchQueue"},
            {1, nullptr, "ClearDebugLaunchQueue"},
            {2, nullptr, "GetNsoInfos"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class ProcessManager final : public ServiceFramework<ProcessManager> {
public:
    explicit ProcessManager() : ServiceFramework{"ldr:pm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateProcess"},
            {1, nullptr, "GetProgramInfo"},
            {2, nullptr, "RegisterTitle"},
            {3, nullptr, "UnregisterTitle"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class Shell final : public ServiceFramework<Shell> {
public:
    explicit Shell() : ServiceFramework{"ldr:shel"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "AddProcessToLaunchQueue"},
            {1, nullptr, "ClearLaunchQueue"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class RelocatableObject final : public ServiceFramework<RelocatableObject> {
public:
    explicit RelocatableObject() : ServiceFramework{"ldr:ro"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &RelocatableObject::LoadNro, "LoadNro"},
            {1, nullptr, "UnloadNro"},
            {2, &RelocatableObject::LoadNrr, "LoadNrr"},
            {3, nullptr, "UnloadNrr"},
            {4, &RelocatableObject::Initialize, "Initialize"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    void LoadNrr(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        rp.Skip(2, false);
        const VAddr nrr_addr{rp.Pop<VAddr>()};
        const u64 nrr_size{rp.Pop<u64>()};

        if (!initialized) {
            LOG_ERROR(Service_LDR, "LDR:RO not initialized before use!");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_NOT_INITIALIZED);
            return;
        }

        if (nro.size() >= MAXIMUM_LOADED_RO) {
            LOG_ERROR(Service_LDR, "Loading new NRR would exceed the maximum number of loaded NRRs "
                                   "(0x40)! Failing...");
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_MAXIMUM_NRR);
            return;
        }

        // NRR Address does not fall on 0x1000 byte boundary
        if ((nrr_addr & 0xFFF) != 0) {
            LOG_ERROR(Service_LDR, "NRR Address has invalid alignment (actual {:016X})!", nrr_addr);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_ALIGNMENT);
            return;
        }

        // NRR Size is zero or causes overflow
        if (nrr_addr + nrr_size <= nrr_addr || nrr_size == 0 || (nrr_size & 0xFFF) != 0) {
            LOG_ERROR(Service_LDR, "NRR Size is invalid! (nrr_address={:016X}, nrr_size={:016X})",
                      nrr_addr, nrr_size);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_SIZE);
            return;
        }
        // Read NRR data from memory
        std::vector<u8> nrr_data(nrr_size);
        Memory::ReadBlock(nrr_addr, nrr_data.data(), nrr_size);
        NRRHeader header;
        std::memcpy(&header, nrr_data.data(), sizeof(NRRHeader));

        if (header.magic != Common::MakeMagic('N', 'R', 'R', '0')) {
            LOG_ERROR(Service_LDR, "NRR did not have magic 'NRR0' (actual {:08X})!", header.magic);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_NRR);
            return;
        }

        if (header.size != nrr_size) {
            LOG_ERROR(Service_LDR,
                      "NRR header reported size did not match LoadNrr parameter size! "
                      "(header_size={:016X}, loadnrr_size={:016X})",
                      header.size, nrr_size);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_SIZE);
            return;
        }

        if (Core::CurrentProcess()->GetTitleID() != header.title_id) {
            LOG_ERROR(Service_LDR,
                      "Attempting to load NRR with title ID other than current process. (actual "
                      "{:016X})!",
                      header.title_id);
            IPC::ResponseBuilder rb{ctx, 2};
            rb.Push(ERROR_INVALID_NRR);
            return;
        }

        std::vector<SHA256Hash> hashes;
        for (std::size_t i = header.hash_offset;
             i < (header.hash_offset + (header.hash_count << 5)); i += 8) {
            hashes.emplace_back();
            std::memcpy(hashes.back().data(), nrr_data.data() + i, sizeof(SHA256Hash));
        }

        nrr.insert_or_assign(nrr_addr, std::move(hashes));

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_LDR, "(STUBBED) called");
    }

    void LoadNro(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        rp.Skip(2, false);
        const VAddr nro_addr{rp.Pop<VAddr>()};
        const u64 nro_size{rp.Pop<u64>()};
        const VAddr bss_addr{rp.Pop<VAddr>()};
        const u64 bss_size{rp.Pop<u64>()};

        // Read NRO data from memory
        std::vector<u8> nro_data(nro_size);
        Memory::ReadBlock(nro_addr, nro_data.data(), nro_size);

        // Load NRO as new executable module
        const VAddr addr{*Core::CurrentProcess()->VMManager().FindFreeRegion(nro_size + bss_size)};
        Loader::AppLoader_NRO::LoadNro(nro_data, fmt::format("nro-{:08x}", addr), addr);

        // TODO(bunnei): This is an incomplete implementation. It was tested with Super Mario Party.
        // It is currently missing:
        // - Signature checks with LoadNRR
        // - Checking if a module has already been loaded
        // - Using/validating BSS, etc. params (these are used from NRO header instead)
        // - Error checking
        // - ...Probably other things

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.Push(addr);
        LOG_WARNING(Service_LDR, "(STUBBED) called");
    }

    void Initialize(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_LDR, "(STUBBED) called");
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<DebugMonitor>()->InstallAsService(sm);
    std::make_shared<ProcessManager>()->InstallAsService(sm);
    std::make_shared<Shell>()->InstallAsService(sm);
    std::make_shared<RelocatableObject>()->InstallAsService(sm);
}

} // namespace Service::LDR
