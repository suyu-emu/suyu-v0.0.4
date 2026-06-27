// Temporary diagnostic flag for GPU stall debugging
#pragma once
#include <atomic>

namespace Service::Nvidia {
inline std::atomic<int> g_gpfifo_debug_counter{0};
}
