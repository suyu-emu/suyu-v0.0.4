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

class BasicSetting {
protected:
    explicit BasicSetting() = default;

public:
    virtual ~BasicSetting() = default;

    virtual Category Category() const = 0;
    virtual constexpr bool Switchable() const = 0;
    virtual std::string ToString() const = 0;
    virtual std::string ToStringGlobal() const;
    virtual void LoadString(const std::string& load) = 0;
    virtual std::string Canonicalize() const = 0;
    virtual const std::string& GetLabel() const = 0;
    virtual std::string DefaultToString() const = 0;
    virtual bool Save() const = 0;
    virtual std::type_index TypeId() const = 0;
    virtual constexpr bool IsEnum() const = 0;
    virtual bool RuntimeModfiable() const = 0;
    virtual void SetGlobal(bool global) {}
    virtual constexpr u32 Id() const = 0;
    virtual std::string MinVal() const = 0;
    virtual std::string MaxVal() const = 0;
    virtual bool UsingGlobal() const;
};

class Linkage {
public:
    explicit Linkage(u32 initial_count = 0);
    ~Linkage();
    std::map<Category, std::forward_list<BasicSetting*>> by_category{};
    std::vector<std::function<void()>> restore_functions{};
    u32 count;
};

} // namespace Settings
