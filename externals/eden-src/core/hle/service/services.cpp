// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/services.h"

#include "core/hle/service/acc/acc.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/aoc/addon_content_manager.h"
#include "core/hle/service/apm/apm.h"
#include "core/hle/service/audio/audio.h"
#include "core/hle/service/bcat/bcat.h"
#include "core/hle/service/bpc/bpc.h"
#include "core/hle/service/btdrv/btdrv.h"
#include "core/hle/service/btm/btm.h"
#include "core/hle/service/caps/caps.h"
#include "core/hle/service/erpt/erpt.h"
#include "core/hle/service/es/es.h"
#include "core/hle/service/eupld/eupld.h"
#include "core/hle/service/fatal/fatal.h"
#include "core/hle/service/fgm/fgm.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/friend/friend.h"
#include "core/hle/service/glue/glue.h"
#include "core/hle/service/grc/grc.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/jit/jit.h"
#include "core/hle/service/lbl/lbl.h"
#include "core/hle/service/ldn/ldn.h"
#include "core/hle/service/ldr/ldr.h"
#include "core/hle/service/lm/lm.h"
#include "core/hle/service/mig/mig.h"
#include "core/hle/service/mii/mii.h"
#include "core/hle/service/mm/mm_u.h"
#include "core/hle/service/mnpp/mnpp_app.h"
#include "core/hle/service/ncm/ncm.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfp/nfp.h"
#include "core/hle/service/ngc/ngc.h"
#include "core/hle/service/nifm/nifm.h"
#include "core/hle/service/nim/nim.h"
#include "core/hle/service/npns/npns.h"
#include "core/hle/service/ns/ns.h"
#include "core/hle/service/nvdrv/nvdrv.h"
#include "core/hle/service/nvnflinger/nvnflinger.h"
#include "core/hle/service/olsc/olsc.h"
#include "core/hle/service/omm/omm.h"
#include "core/hle/service/pcie/pcie.h"
#include "core/hle/service/pctl/pctl.h"
#include "core/hle/service/pcv/pcv.h"
#include "core/hle/service/pm/pm.h"
#include "core/hle/service/prepo/prepo.h"
#include "core/hle/service/psc/psc.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ro/ro.h"
#include "core/hle/service/service.h"
#include "core/hle/service/set/settings.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sockets/sockets.h"
#include "core/hle/service/spl/spl_module.h"
#include "core/hle/service/ssl/ssl.h"
#include "core/hle/service/usb/usb.h"
#include "core/hle/service/vi/vi.h"

namespace Service {

Services::Services(std::shared_ptr<SM::ServiceManager>& sm, Core::System& system,
                   std::stop_token token) {
    auto& kernel = system.Kernel();

    system.GetFileSystemController().CreateFactories(*system.GetFilesystem(), false);

    // Just a quick C++ lesson
    // Capturing lambdas will silently create new variables for the objects referenced via <ident> = <expr>
    // and create a `auto&` sorts of for `&`; with all your usual reference shenanigans.
    // Do not be confused, `std::function<>` will allocate into the heap and will do so most of the time
    // The heap is where we'd expect our "stored" values to be placed at.
    //
    // Eventually we'd need a "heapless" solution so the overhead is nil - but again a good starting point
    // is removing all the cold clones ;)

    // BEGONE cold clones of lambdas, for I have merged you all into a SINGLE lambda instead of
    // spamming lambdas like it's some kind of lambda calculus class
    for (auto const& e : std::vector<std::pair<std::string_view, void (*)(Core::System&)>>{
        {"audio",      &Audio::LoopProcess},
        {"FS",         &FileSystem::LoopProcess},
        {"jit",        &JIT::LoopProcess},
        {"ldn",        &LDN::LoopProcess},
        {"Loader",     &LDR::LoopProcess},
        {"nvservices", &Nvidia::LoopProcess},
        {"bsdsocket",  &Sockets::LoopProcess},
    })
        kernel.RunOnHostCoreProcess(std::string(e.first), [&system, f = e.second] { f(system); }).detach();
    kernel.RunOnHostCoreProcess("vi",         [&, token] { VI::LoopProcess(system, token); }).detach();
    // Avoid cold clones of lambdas -- succintly
    for (auto const& e : std::vector<std::pair<std::string_view, void (*)(Core::System&)>>{
        {"sm",         &SM::LoopProcess},
        {"account",    &Account::LoopProcess},
        {"am",         &AM::LoopProcess},
        {"aoc",        &AOC::LoopProcess},
        {"apm",        &APM::LoopProcess},
        {"bcat",       &BCAT::LoopProcess},
        {"bpc",        &BPC::LoopProcess},
        {"btdrv",      &BtDrv::LoopProcess},
        {"btm",        &BTM::LoopProcess},
        {"capsrv",     &Capture::LoopProcess},
        {"erpt",       &ERPT::LoopProcess},
        {"es",         &ES::LoopProcess},
        {"eupld",      &EUPLD::LoopProcess},
        {"fatal",      &Fatal::LoopProcess},
        {"fgm",        &FGM::LoopProcess},
        {"friends",    &Friend::LoopProcess},
        {"settings",   &Set::LoopProcess},
        {"psc",        &PSC::LoopProcess},
        {"glue",       &Glue::LoopProcess},
        {"grc",        &GRC::LoopProcess},
        {"hid",        &HID::LoopProcess},
        {"lbl",        &LBL::LoopProcess},
        {"LogManager.Prod", &LM::LoopProcess},
        {"mig",        &Migration::LoopProcess},
        {"mii",        &Mii::LoopProcess},
        {"mm",         &MM::LoopProcess},
        {"mnpp",       &MNPP::LoopProcess},
        {"nvnflinger", &Nvnflinger::LoopProcess},
        {"NCM",        &NCM::LoopProcess},
        {"nfc",        &NFC::LoopProcess},
        {"nfp",        &NFP::LoopProcess},
        {"ngc",        &NGC::LoopProcess},
        {"nifm",       &NIFM::LoopProcess},
        {"nim",        &NIM::LoopProcess},
        {"npns",       &NPNS::LoopProcess},
        {"ns",         &NS::LoopProcess},
        {"olsc",       &OLSC::LoopProcess},
        {"omm",        &OMM::LoopProcess},
        {"pcie",       &PCIe::LoopProcess},
        {"pctl",       &PCTL::LoopProcess},
        {"pcv",        &PCV::LoopProcess},
        {"prepo",      &PlayReport::LoopProcess},
        {"ProcessManager", &PM::LoopProcess},
        {"ptm",        &PTM::LoopProcess},
        {"ro",         &RO::LoopProcess},
        {"spl",        &SPL::LoopProcess},
        {"ssl",        &SSL::LoopProcess},
        {"usb",        &USB::LoopProcess}
    })
        kernel.RunOnGuestCoreProcess(std::string(e.first), [&system, f = e.second] { f(system); });
}

} // namespace Service
