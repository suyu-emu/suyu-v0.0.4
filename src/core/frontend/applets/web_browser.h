// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>

namespace Core::Frontend {

class WebBrowserApplet {
public:
    virtual ~WebBrowserApplet();
};

class DefaultWebBrowserApplet final : public WebBrowserApplet {
public:
    ~DefaultWebBrowserApplet() override;
};

} // namespace Core::Frontend
