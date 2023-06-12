// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <forward_list>
#include <functional>
#include <map>
#include <string>
#include <typeindex>
#include "common/common_types.h"

namespace Settings {

enum class Category : u32 {
    Audio,
    Core,
    Cpu,
    CpuDebug,
    CpuUnsafe,
    Renderer,
    RendererAdvanced,
    RendererDebug,
    System,
    SystemAudio,
    DataStorage,
    Debugging,
    DebuggingGraphics,
    Miscellaneous,
    Network,
    WebService,
    AddOns,
    Controls,
    Ui,
    UiGeneral,
    UiLayout,
    UiGameList,
    Screenshots,
    Shortcuts,
    Multiplayer,
    Services,
    Paths,
    MaxEnum,
};

class BasicSetting;

class Linkage {
public:
    explicit Linkage(u32 initial_count = 0);
    ~Linkage();
    std::map<Category, std::forward_list<BasicSetting*>> by_category{};
    std::vector<std::function<void()>> restore_functions{};
    u32 count;
};

class BasicSetting {
protected:
    explicit BasicSetting(Linkage& linkage, const std::string& name, enum Category category_,
                          bool save_, bool runtime_modifiable_);

public:
    virtual ~BasicSetting();

    /* Data retrieval */

    [[nodiscard]] virtual std::string ToString() const = 0;
    [[nodiscard]] virtual std::string ToStringGlobal() const;
    [[nodiscard]] virtual std::string DefaultToString() const = 0;
    [[nodiscard]] virtual std::string MinVal() const = 0;
    [[nodiscard]] virtual std::string MaxVal() const = 0;
    virtual void LoadString(const std::string& load) = 0;
    [[nodiscard]] virtual std::string Canonicalize() const = 0;

    /* Identification */

    [[nodiscard]] virtual std::type_index TypeId() const = 0;
    [[nodiscard]] virtual constexpr bool IsEnum() const = 0;
    /**
     * Returns whether the current setting is Switchable.
     *
     * @returns If the setting is a SwitchableSetting
     */
    [[nodiscard]] virtual constexpr bool Switchable() const {
        return false;
    }
    /**
     * Returns the save preference of the setting i.e. when saving or reading the setting from a
     * frontend, whether this setting should be skipped.
     *
     * @returns The save preference
     */
    [[nodiscard]] bool Save() const;
    [[nodiscard]] bool RuntimeModfiable() const;
    [[nodiscard]] constexpr u32 Id() const {
        return id;
    }
    /**
     * Returns the setting's category AKA INI group.
     *
     * @returns The setting's category
     */
    [[nodiscard]] Category Category() const;
    /**
     * Returns the label this setting was created with.
     *
     * @returns A reference to the label
     */
    [[nodiscard]] const std::string& GetLabel() const;

    /* Switchable settings */

    virtual void SetGlobal(bool global);
    [[nodiscard]] virtual bool UsingGlobal() const;

private:
    const std::string label;      ///< The setting's label
    const enum Category category; ///< The setting's category AKA INI group
    const u32 id;
    const bool save;
    const bool runtime_modifiable;
};

} // namespace Settings
