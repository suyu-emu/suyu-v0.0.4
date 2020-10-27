// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <bitset>
#include "common/assert.h"
#include "common/bit_util.h"
#include "core/memory.h"
#include "video_core/command_classes/nvdec.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"

namespace Tegra {

Nvdec::Nvdec(GPU& gpu_) : gpu(gpu_), codec(std::make_unique<Codec>(gpu)) {}

Nvdec::~Nvdec() = default;

void Nvdec::ProcessMethod(Nvdec::Method method, const std::vector<u32>& arguments) {
    if (method == Method::SetVideoCodec) {
        codec->StateWrite(static_cast<u32>(method), arguments[0]);
    } else {
        codec->StateWrite(static_cast<u32>(method), static_cast<u64>(arguments[0]) << 8);
    }

    switch (method) {
    case Method::SetVideoCodec:
        codec->SetTargetCodec(static_cast<NvdecCommon::VideoCodec>(arguments[0]));
        break;
    case Method::Execute:
        Execute();
        break;
    }
}

AVFrame* Nvdec::GetFrame() {
    return codec->GetCurrentFrame();
}

const AVFrame* Nvdec::GetFrame() const {
    return codec->GetCurrentFrame();
}

void Nvdec::Execute() {
    switch (codec->GetCurrentCodec()) {
    case NvdecCommon::VideoCodec::H264:
    case NvdecCommon::VideoCodec::Vp9:
        codec->Decode();
        break;
    default:
        UNIMPLEMENTED_MSG("Unknown codec {}", static_cast<u32>(codec->GetCurrentCodec()));
        break;
    }
}

} // namespace Tegra
