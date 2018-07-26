// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/sockets/bsd.h"
#include "core/hle/service/sockets/ethc.h"
#include "core/hle/service/sockets/nsd.h"
#include "core/hle/service/sockets/sfdnsres.h"
#include "core/hle/service/sockets/sockets.h"

namespace Service::Sockets {

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<BSD>("bsd:s")->InstallAsService(service_manager);
    std::make_shared<BSD>("bsd:u")->InstallAsService(service_manager);
    std::make_shared<BSDCFG>()->InstallAsService(service_manager);

    std::make_shared<ETHC_C>()->InstallAsService(service_manager);
    std::make_shared<ETHC_I>()->InstallAsService(service_manager);

    std::make_shared<NSD>("nsd:a")->InstallAsService(service_manager);
    std::make_shared<NSD>("nsd:u")->InstallAsService(service_manager);

    std::make_shared<SFDNSRES>()->InstallAsService(service_manager);
}

} // namespace Service::Sockets
