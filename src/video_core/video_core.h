// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

class EmuWindow;

namespace VideoCore {

class RendererBase;

/**
 * Creates a renderer instance.
 *
 * @note The returned renderer instance is simply allocated. Its Init()
 *       function still needs to be called to fully complete its setup.
 */
std::unique_ptr<RendererBase> CreateRenderer(EmuWindow& emu_window);

} // namespace VideoCore
