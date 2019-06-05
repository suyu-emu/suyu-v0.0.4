// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <optional>
#include "common/common_types.h"

namespace Core::Frontend {

class ParentalControlsApplet {
public:
    virtual ~ParentalControlsApplet();

    virtual void VerifyPIN(std::function<void(bool)> finished,
                           bool suspend_future_verification_temporarily) = 0;
    virtual void VerifyPINForSettings(std::function<void(bool)> finished) = 0;
    virtual void RegisterPIN(std::function<void()> finished) = 0;
    virtual void ChangePIN(std::function<void()> finished) = 0;
};

class DefaultParentalControlsApplet final : public ParentalControlsApplet {
public:
    ~DefaultParentalControlsApplet() override;

    void VerifyPIN(std::function<void(bool)> finished,
                   bool suspend_future_verification_temporarily) override;
    void VerifyPINForSettings(std::function<void(bool)> finished) override;
    void RegisterPIN(std::function<void()> finished) override;
    void ChangePIN(std::function<void()> finished) override;
};

class PhotoViewerApplet {
public:
    virtual ~PhotoViewerApplet();

    virtual void ShowPhotosForApplication(u64 title_id, std::function<void()> finished) const = 0;
    virtual void ShowAllPhotos(std::function<void()> finished) const = 0;
};

class DefaultPhotoViewerApplet final : public PhotoViewerApplet {
public:
    ~DefaultPhotoViewerApplet() override;

    void ShowPhotosForApplication(u64 title_id, std::function<void()> finished) const override;
    void ShowAllPhotos(std::function<void()> finished) const override;
};

class ECommerceApplet {
public:
    virtual ~ECommerceApplet();

    virtual void ShowApplicationInformation(std::function<void()> finished, u64 title_id,
                                            std::optional<u128> user_id = {},
                                            std::optional<bool> full_display = {},
                                            std::optional<std::string> extra_parameter = {}) = 0;
    virtual void ShowAddOnContentList(std::function<void()> finished, u64 title_id,
                                      std::optional<u128> user_id = {},
                                      std::optional<bool> full_display = {}) = 0;
    virtual void ShowSubscriptionList(std::function<void()> finished, u64 title_id,
                                      std::optional<u128> user_id = {}) = 0;
    virtual void ShowConsumableItemList(std::function<void()> finished, u64 title_id,
                                        std::optional<u128> user_id = {}) = 0;
    virtual void ShowShopHome(std::function<void()> finished, u128 user_id, bool full_display) = 0;
    virtual void ShowSettings(std::function<void()> finished, u128 user_id, bool full_display) = 0;
};

class DefaultECommerceApplet : public ECommerceApplet {
public:
    ~DefaultECommerceApplet() override;

    void ShowApplicationInformation(std::function<void()> finished, u64 title_id,
                                    std::optional<u128> user_id, std::optional<bool> full_display,
                                    std::optional<std::string> extra_parameter) override;
    void ShowAddOnContentList(std::function<void()> finished, u64 title_id,
                              std::optional<u128> user_id,
                              std::optional<bool> full_display) override;
    void ShowSubscriptionList(std::function<void()> finished, u64 title_id,
                              std::optional<u128> user_id) override;
    void ShowConsumableItemList(std::function<void()> finished, u64 title_id,
                                std::optional<u128> user_id) override;
    void ShowShopHome(std::function<void()> finished, u128 user_id, bool full_display) override;
    void ShowSettings(std::function<void()> finished, u128 user_id, bool full_display) override;
};

} // namespace Core::Frontend
