// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string_view>

#include "core/hle/service/am/applets/web_types.h"

namespace Core::Frontend {

class WebBrowserApplet {
public:
    virtual ~WebBrowserApplet();

    virtual void OpenLocalWebPage(
        std::string_view local_url, std::function<void()> extract_romfs_callback,
        std::function<void(Service::AM::Applets::WebExitReason, std::string)> callback) const = 0;

    virtual void OpenExternalWebPage(
        std::string_view external_url,
        std::function<void(Service::AM::Applets::WebExitReason, std::string)> callback) const = 0;
};

class DefaultWebBrowserApplet final : public WebBrowserApplet {
public:
    ~DefaultWebBrowserApplet() override;

    void OpenLocalWebPage(std::string_view local_url, std::function<void()> extract_romfs_callback,
                          std::function<void(Service::AM::Applets::WebExitReason, std::string)>
                              callback) const override;

    void OpenExternalWebPage(std::string_view external_url,
                             std::function<void(Service::AM::Applets::WebExitReason, std::string)>
                                 callback) const override;
};

} // namespace Core::Frontend
