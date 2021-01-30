// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Core::Frontend {

class SoftwareKeyboardApplet {
public:
    virtual ~SoftwareKeyboardApplet();
};

class DefaultSoftwareKeyboardApplet final : public SoftwareKeyboardApplet {
public:
};

} // namespace Core::Frontend
