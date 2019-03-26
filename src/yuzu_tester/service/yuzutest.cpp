// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/string_util.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "yuzu_tester/service/yuzutest.h"

namespace Service::Yuzu {

constexpr u64 SERVICE_VERSION = 1;

class YuzuTest final : public ServiceFramework<YuzuTest> {
public:
    explicit YuzuTest(std::string data, std::function<void(u32, std::string)> finish_callback)
        : ServiceFramework{"yuzutest"}, data(std::move(data)),
          finish_callback(std::move(finish_callback)) {
        static const FunctionInfo functions[] = {
            {0, &YuzuTest::Initialize, "Initialize"},
            {1, &YuzuTest::GetServiceVersion, "GetServiceVersion"},
            {2, &YuzuTest::GetData, "GetData"},
            {3, &YuzuTest::SetResultCode, "SetResultCode"},
            {4, &YuzuTest::SetResultData, "SetResultData"},
            {5, &YuzuTest::Finish, "Finish"},
        };

        RegisterHandlers(functions);
    }

private:
    void Initialize(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Frontend, "called");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void GetServiceVersion(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Frontend, "called");
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.Push(SERVICE_VERSION);
    }

    void GetData(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Frontend, "called");
        const auto size = ctx.GetWriteBufferSize();
        const auto write_size = std::min(size, data.size());
        ctx.WriteBuffer(data.data(), write_size);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(write_size);
    }

    void SetResultCode(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto code = rp.PopRaw<u32>();

        LOG_INFO(Frontend, "called with result_code={:08X}", code);
        result_code = code;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void SetResultData(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto buffer = ctx.ReadBuffer();
        std::string data = Common::StringFromFixedZeroTerminatedBuffer(
            reinterpret_cast<const char*>(buffer.data()), buffer.size());

        LOG_INFO(Frontend, "called with string={}", data);
        result_string = data;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void Finish(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Frontend, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);

        finish_callback(result_code, result_string);
    }

    std::string data;

    u32 result_code = 0;
    std::string result_string;

    std::function<void(u64, std::string)> finish_callback;
};

void InstallInterfaces(SM::ServiceManager& sm, std::string data,
                       std::function<void(u32, std::string)> finish_callback) {
    std::make_shared<YuzuTest>(data, finish_callback)->InstallAsService(sm);
}

} // namespace Service::Yuzu
