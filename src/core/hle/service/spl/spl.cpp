// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/spl/spl.h"

namespace Service::SPL {

SPL::SPL(std::shared_ptr<Module> module) : Module::Interface(std::move(module), "spl:") {
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetConfig"},
        {1, nullptr, "UserExpMod"},
        {2, nullptr, "GenerateAesKek"},
        {3, nullptr, "LoadAesKey"},
        {4, nullptr, "GenerateAesKey"},
        {5, nullptr, "SetConfig"},
        {7, &SPL::GetRandomBytes, "GetRandomBytes"},
        {9, nullptr, "LoadSecureExpModKey"},
        {10, nullptr, "SecureExpMod"},
        {11, nullptr, "IsDevelopment"},
        {12, nullptr, "GenerateSpecificAesKey"},
        {13, nullptr, "DecryptPrivk"},
        {14, nullptr, "DecryptAesKey"},
        {15, nullptr, "DecryptAesCtr"},
        {16, nullptr, "ComputeCmac"},
        {17, nullptr, "LoadRsaOaepKey"},
        {18, nullptr, "UnwrapRsaOaepWrappedTitleKey"},
        {19, nullptr, "LoadTitleKey"},
        {20, nullptr, "UnwrapAesWrappedTitleKey"},
        {21, nullptr, "LockAesEngine"},
        {22, nullptr, "UnlockAesEngine"},
        {23, nullptr, "GetSplWaitEvent"},
        {24, nullptr, "SetSharedData"},
        {25, nullptr, "GetSharedData"},
        {26, nullptr, "ImportSslRsaKey"},
        {27, nullptr, "SecureExpModWithSslKey"},
        {28, nullptr, "ImportEsRsaKey"},
        {29, nullptr, "SecureExpModWithEsKey"},
        {30, nullptr, "EncryptManuRsaKeyForImport"},
        {31, nullptr, "GetPackage2Hash"},
    };
    RegisterHandlers(functions);
}

SPL::~SPL() = default;

} // namespace Service::SPL
