// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_core/sink/libretro_sink.h"

namespace AudioCore::Sink {

// Defined out-of-line deliberately. As a function-local static inside an
// inline header function, the producer (audio_core) and the consumer
// (libretro_core) each ended up with their own copy of the queue, so samples
// were pushed into one instance and drained from another - the sink filled
// correctly while retro_run always saw an empty queue. A single definition in
// one translation unit is what guarantees both sides share it.
LibretroSampleQueue& LibretroSampleQueue::Instance() {
    static LibretroSampleQueue instance;
    return instance;
}

} // namespace AudioCore::Sink
