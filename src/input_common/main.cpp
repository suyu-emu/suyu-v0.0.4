// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <thread>
#include "common/input.h"
#include "common/param_package.h"
#include "input_common/drivers/gc_adapter.h"
#include "input_common/drivers/keyboard.h"
#include "input_common/drivers/mouse.h"
#include "input_common/drivers/tas_input.h"
#include "input_common/drivers/touch_screen.h"
#include "input_common/drivers/udp_client.h"
#include "input_common/helpers/stick_from_buttons.h"
#include "input_common/helpers/touch_from_buttons.h"
#include "input_common/input_engine.h"
#include "input_common/input_mapping.h"
#include "input_common/input_poller.h"
#include "input_common/main.h"
#ifdef HAVE_SDL2
#include "input_common/drivers/sdl_driver.h"
#endif

namespace InputCommon {

struct InputSubsystem::Impl {
    void Initialize() {
        mapping_factory = std::make_shared<MappingFactory>();
        MappingCallback mapping_callback{[this](MappingData data) { RegisterInput(data); }};

        keyboard = std::make_shared<Keyboard>("keyboard");
        keyboard->SetMappingCallback(mapping_callback);
        keyboard_factory = std::make_shared<InputFactory>(keyboard);
        Input::RegisterFactory<Input::InputDevice>(keyboard->GetEngineName(), keyboard_factory);

        mouse = std::make_shared<Mouse>("mouse");
        mouse->SetMappingCallback(mapping_callback);
        mouse_factory = std::make_shared<InputFactory>(mouse);
        Input::RegisterFactory<Input::InputDevice>(mouse->GetEngineName(), mouse_factory);

        touch_screen = std::make_shared<TouchScreen>("touch");
        touch_screen_factory = std::make_shared<InputFactory>(touch_screen);
        Input::RegisterFactory<Input::InputDevice>(touch_screen->GetEngineName(),
                                                   touch_screen_factory);

        gcadapter = std::make_shared<GCAdapter>("gcpad");
        gcadapter->SetMappingCallback(mapping_callback);
        gcadapter_factory = std::make_shared<InputFactory>(gcadapter);
        Input::RegisterFactory<Input::InputDevice>(gcadapter->GetEngineName(), gcadapter_factory);

        udp_client = std::make_shared<CemuhookUDP::UDPClient>("cemuhookudp");
        udp_client->SetMappingCallback(mapping_callback);
        udp_client_factory = std::make_shared<InputFactory>(udp_client);
        Input::RegisterFactory<Input::InputDevice>(udp_client->GetEngineName(), udp_client_factory);

        tas_input = std::make_shared<TasInput::Tas>("tas");
        tas_input->SetMappingCallback(mapping_callback);
        tas_input_factory = std::make_shared<InputFactory>(tas_input);
        Input::RegisterFactory<Input::InputDevice>(tas_input->GetEngineName(), tas_input_factory);

#ifdef HAVE_SDL2
        sdl = std::make_shared<SDLDriver>("sdl");
        sdl->SetMappingCallback(mapping_callback);
        sdl_factory = std::make_shared<InputFactory>(sdl);
        Input::RegisterFactory<Input::InputDevice>(sdl->GetEngineName(), sdl_factory);
#endif

        Input::RegisterFactory<Input::InputDevice>("touch_from_button",
                                                   std::make_shared<TouchFromButton>());
        Input::RegisterFactory<Input::InputDevice>("analog_from_button",
                                                   std::make_shared<StickFromButton>());
    }

    void Shutdown() {
        Input::UnregisterFactory<Input::InputDevice>(keyboard->GetEngineName());
        keyboard.reset();

        Input::UnregisterFactory<Input::InputDevice>(mouse->GetEngineName());
        mouse.reset();

        Input::UnregisterFactory<Input::InputDevice>(touch_screen->GetEngineName());
        touch_screen.reset();

        Input::UnregisterFactory<Input::InputDevice>(gcadapter->GetEngineName());
        gcadapter.reset();

        Input::UnregisterFactory<Input::InputDevice>(udp_client->GetEngineName());
        udp_client.reset();

        Input::UnregisterFactory<Input::InputDevice>(tas_input->GetEngineName());
        tas_input.reset();

#ifdef HAVE_SDL2
        Input::UnregisterFactory<Input::InputDevice>(sdl->GetEngineName());
        sdl.reset();
#endif

        Input::UnregisterFactory<Input::InputDevice>("touch_from_button");
        Input::UnregisterFactory<Input::InputDevice>("analog_from_button");
    }

    [[nodiscard]] std::vector<Common::ParamPackage> GetInputDevices() const {
        std::vector<Common::ParamPackage> devices = {
            Common::ParamPackage{{"display", "Any"}, {"engine", "any"}},
        };

        auto keyboard_devices = keyboard->GetInputDevices();
        devices.insert(devices.end(), keyboard_devices.begin(), keyboard_devices.end());
        auto mouse_devices = mouse->GetInputDevices();
        devices.insert(devices.end(), mouse_devices.begin(), mouse_devices.end());
        auto gcadapter_devices = gcadapter->GetInputDevices();
        devices.insert(devices.end(), gcadapter_devices.begin(), gcadapter_devices.end());
        auto tas_input_devices = tas_input->GetInputDevices();
        devices.insert(devices.end(), tas_input_devices.begin(), tas_input_devices.end());
#ifdef HAVE_SDL2
        auto sdl_devices = sdl->GetInputDevices();
        devices.insert(devices.end(), sdl_devices.begin(), sdl_devices.end());
#endif

        return devices;
    }

    [[nodiscard]] AnalogMapping GetAnalogMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("engine") || params.Get("engine", "") == "any") {
            return {};
        }
        const std::string engine = params.Get("engine", "");
        if (engine == gcadapter->GetEngineName()) {
            return gcadapter->GetAnalogMappingForDevice(params);
        }
        if (engine == tas_input->GetEngineName()) {
            return tas_input->GetAnalogMappingForDevice(params);
        }
#ifdef HAVE_SDL2
        if (engine == sdl->GetEngineName()) {
            return sdl->GetAnalogMappingForDevice(params);
        }
#endif
        return {};
    }

    [[nodiscard]] ButtonMapping GetButtonMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("engine") || params.Get("engine", "") == "any") {
            return {};
        }
        const std::string engine = params.Get("engine", "");
        if (engine == gcadapter->GetEngineName()) {
            return gcadapter->GetButtonMappingForDevice(params);
        }
        if (engine == tas_input->GetEngineName()) {
            return tas_input->GetButtonMappingForDevice(params);
        }
#ifdef HAVE_SDL2
        if (engine == sdl->GetEngineName()) {
            return sdl->GetButtonMappingForDevice(params);
        }
#endif
        return {};
    }

    [[nodiscard]] MotionMapping GetMotionMappingForDevice(
        const Common::ParamPackage& params) const {
        if (!params.Has("engine") || params.Get("engine", "") == "any") {
            return {};
        }
        const std::string engine = params.Get("engine", "");
        if (engine == gcadapter->GetEngineName()) {
            return gcadapter->GetMotionMappingForDevice(params);
        }
#ifdef HAVE_SDL2
        if (engine == sdl->GetEngineName()) {
            return sdl->GetMotionMappingForDevice(params);
        }
#endif
        return {};
    }

    std::string GetButtonName(const Common::ParamPackage& params) const {
        if (!params.Has("engine") || params.Get("engine", "") == "any") {
            return "Unknown";
        }
        const std::string engine = params.Get("engine", "");
        if (engine == mouse->GetEngineName()) {
            return mouse->GetUIName(params);
        }
        if (engine == gcadapter->GetEngineName()) {
            return gcadapter->GetUIName(params);
        }
        if (engine == udp_client->GetEngineName()) {
            return udp_client->GetUIName(params);
        }
        if (engine == tas_input->GetEngineName()) {
            return tas_input->GetUIName(params);
        }
#ifdef HAVE_SDL2
        if (engine == sdl->GetEngineName()) {
            return sdl->GetUIName(params);
        }
#endif
        return "Bad engine";
    }

    bool IsController(const Common::ParamPackage& params) {
        const std::string engine = params.Get("engine", "");
        if (engine == mouse->GetEngineName()) {
            return true;
        }
        if (engine == gcadapter->GetEngineName()) {
            return true;
        }
        if (engine == tas_input->GetEngineName()) {
            return true;
        }
#ifdef HAVE_SDL2
        if (engine == sdl->GetEngineName()) {
            return true;
        }
#endif
        return false;
    }

    void BeginConfiguration() {
        keyboard->BeginConfiguration();
        mouse->BeginConfiguration();
        gcadapter->BeginConfiguration();
        udp_client->BeginConfiguration();
#ifdef HAVE_SDL2
        sdl->BeginConfiguration();
#endif
    }

    void EndConfiguration() {
        keyboard->EndConfiguration();
        mouse->EndConfiguration();
        gcadapter->EndConfiguration();
        udp_client->EndConfiguration();
#ifdef HAVE_SDL2
        sdl->EndConfiguration();
#endif
    }

    void RegisterInput(MappingData data) {
        mapping_factory->RegisterInput(data);
    }

    std::shared_ptr<MappingFactory> mapping_factory;
    std::shared_ptr<Keyboard> keyboard;
    std::shared_ptr<InputFactory> keyboard_factory;
    std::shared_ptr<Mouse> mouse;
    std::shared_ptr<InputFactory> mouse_factory;
    std::shared_ptr<GCAdapter> gcadapter;
    std::shared_ptr<InputFactory> gcadapter_factory;
    std::shared_ptr<TouchScreen> touch_screen;
    std::shared_ptr<InputFactory> touch_screen_factory;
    std::shared_ptr<CemuhookUDP::UDPClient> udp_client;
    std::shared_ptr<InputFactory> udp_client_factory;
    std::shared_ptr<TasInput::Tas> tas_input;
    std::shared_ptr<InputFactory> tas_input_factory;
#ifdef HAVE_SDL2
    std::shared_ptr<SDLDriver> sdl;
    std::shared_ptr<InputFactory> sdl_factory;
#endif
};

InputSubsystem::InputSubsystem() : impl{std::make_unique<Impl>()} {}

InputSubsystem::~InputSubsystem() = default;

void InputSubsystem::Initialize() {
    impl->Initialize();
}

void InputSubsystem::Shutdown() {
    impl->Shutdown();
}

Keyboard* InputSubsystem::GetKeyboard() {
    return impl->keyboard.get();
}

const Keyboard* InputSubsystem::GetKeyboard() const {
    return impl->keyboard.get();
}

Mouse* InputSubsystem::GetMouse() {
    return impl->mouse.get();
}

const Mouse* InputSubsystem::GetMouse() const {
    return impl->mouse.get();
}

TouchScreen* InputSubsystem::GetTouchScreen() {
    return impl->touch_screen.get();
}

const TouchScreen* InputSubsystem::GetTouchScreen() const {
    return impl->touch_screen.get();
}

TasInput::Tas* InputSubsystem::GetTas() {
    return impl->tas_input.get();
}

const TasInput::Tas* InputSubsystem::GetTas() const {
    return impl->tas_input.get();
}

std::vector<Common::ParamPackage> InputSubsystem::GetInputDevices() const {
    return impl->GetInputDevices();
}

AnalogMapping InputSubsystem::GetAnalogMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetAnalogMappingForDevice(device);
}

ButtonMapping InputSubsystem::GetButtonMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetButtonMappingForDevice(device);
}

MotionMapping InputSubsystem::GetMotionMappingForDevice(const Common::ParamPackage& device) const {
    return impl->GetMotionMappingForDevice(device);
}

std::string InputSubsystem::GetButtonName(const Common::ParamPackage& params) const {
    const std::string toggle = params.Get("toggle", false) ? "~" : "";
    const std::string inverted = params.Get("inverted", false) ? "!" : "";
    const std::string button_name = impl->GetButtonName(params);
    std::string axis_direction = "";
    if (params.Has("axis")) {
        axis_direction = params.Get("invert", "+");
    }
    return fmt::format("{}{}{}{}", toggle, inverted, button_name, axis_direction);
}

bool InputSubsystem::IsController(const Common::ParamPackage& params) const {
    return impl->IsController(params);
}

void InputSubsystem::ReloadInputDevices() {
    impl->udp_client.get()->ReloadSockets();
}

void InputSubsystem::BeginMapping(Polling::InputType type) {
    impl->BeginConfiguration();
    impl->mapping_factory->BeginMapping(type);
}

const Common::ParamPackage InputSubsystem::GetNextInput() const {
    return impl->mapping_factory->GetNextInput();
}

void InputSubsystem::StopMapping() const {
    impl->EndConfiguration();
    impl->mapping_factory->StopMapping();
}

std::string GenerateKeyboardParam(int key_code) {
    Common::ParamPackage param;
    param.Set("engine", "keyboard");
    param.Set("code", key_code);
    param.Set("toggle", false);
    return param.Serialize();
}

std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale) {
    Common::ParamPackage circle_pad_param{
        {"engine", "analog_from_button"},
        {"up", GenerateKeyboardParam(key_up)},
        {"down", GenerateKeyboardParam(key_down)},
        {"left", GenerateKeyboardParam(key_left)},
        {"right", GenerateKeyboardParam(key_right)},
        {"modifier", GenerateKeyboardParam(key_modifier)},
        {"modifier_scale", std::to_string(modifier_scale)},
    };
    return circle_pad_param.Serialize();
}
} // namespace InputCommon
