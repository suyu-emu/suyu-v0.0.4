// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <QWidget>

#include "common/param_package.h"
#include "core/settings.h"
#include "ui_configure_input.h"

class QCheckBox;
class QKeyEvent;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QString;
class QTimer;
class QWidget;

namespace InputCommon {
class InputSubsystem;
}

namespace InputCommon::Polling {
class DevicePoller;
enum class DeviceType;
} // namespace InputCommon::Polling

namespace Ui {
class ConfigureInputPlayer;
}

class ConfigureInputPlayer : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureInputPlayer(QWidget* parent, std::size_t player_index, QWidget* bottom_row,
                                  InputCommon::InputSubsystem* input_subsystem_,
                                  bool debug = false);
    ~ConfigureInputPlayer() override;

    /// Save all button configurations to settings file.
    void ApplyConfiguration();

    /// Update the input devices combobox.
    void UpdateInputDevices();

    /// Restore all buttons to their default values.
    void RestoreDefaults();

    /// Clear all input configuration.
    void ClearAll();

    /// Set the connection state checkbox (used to sync state).
    void ConnectPlayer(bool connected);

signals:
    /// Emitted when this controller is connected by the user.
    void Connected(bool connected);
    /// Emitted when the Handheld mode is selected (undocked with dual joycons attached).
    void HandheldStateChanged(bool is_handheld);
    /// Emitted when the input devices combobox is being refreshed.
    void RefreshInputDevices();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    /// Load configuration settings.
    void LoadConfiguration();

    /// Called when the button was pressed.
    void HandleClick(QPushButton* button,
                     std::function<void(const Common::ParamPackage&)> new_input_setter,
                     InputCommon::Polling::DeviceType type);

    /// Finish polling and configure input using the input_setter.
    void SetPollingResult(const Common::ParamPackage& params, bool abort);

    /// Handle mouse button press events.
    void mousePressEvent(QMouseEvent* event) override;

    /// Handle key press events.
    void keyPressEvent(QKeyEvent* event) override;

    /// Update UI to reflect current configuration.
    void UpdateUI();

    /// Update the controller selection combobox
    void UpdateControllerCombobox();

    /// Update the current controller icon.
    void UpdateControllerIcon();

    /// Hides and disables controller settings based on the current controller type.
    void UpdateControllerAvailableButtons();

    /// Shows or hides motion groupboxes based on the current controller type.
    void UpdateMotionButtons();

    /// Gets the default controller mapping for this device and auto configures the input to match.
    void UpdateMappingWithDefaults();

    std::unique_ptr<Ui::ConfigureInputPlayer> ui;

    std::size_t player_index;
    bool debug;

    InputCommon::InputSubsystem* input_subsystem;

    std::unique_ptr<QTimer> timeout_timer;
    std::unique_ptr<QTimer> poll_timer;

    static constexpr int PLAYER_COUNT = 8;
    std::array<QCheckBox*, PLAYER_COUNT> player_connected_checkbox;

    /// This will be the the setting function when an input is awaiting configuration.
    std::optional<std::function<void(const Common::ParamPackage&)>> input_setter;

    std::array<Common::ParamPackage, Settings::NativeButton::NumButtons> buttons_param;
    std::array<Common::ParamPackage, Settings::NativeAnalog::NumAnalogs> analogs_param;
    std::array<Common::ParamPackage, Settings::NativeMotion::NumMotions> motions_param;

    static constexpr int ANALOG_SUB_BUTTONS_NUM = 4;

    /// Each button input is represented by a QPushButton.
    std::array<QPushButton*, Settings::NativeButton::NumButtons> button_map;

    /// A group of four QPushButtons represent one analog input. The buttons each represent up,
    /// down, left, right, respectively.
    std::array<std::array<QPushButton*, ANALOG_SUB_BUTTONS_NUM>, Settings::NativeAnalog::NumAnalogs>
        analog_map_buttons;

    /// Each motion input is represented by a QPushButton.
    std::array<QPushButton*, Settings::NativeMotion::NumMotions> motion_map;

    std::array<QLabel*, Settings::NativeAnalog::NumAnalogs> analog_map_deadzone_label;
    std::array<QSlider*, Settings::NativeAnalog::NumAnalogs> analog_map_deadzone_slider;
    std::array<QGroupBox*, Settings::NativeAnalog::NumAnalogs> analog_map_modifier_groupbox;
    std::array<QPushButton*, Settings::NativeAnalog::NumAnalogs> analog_map_modifier_button;
    std::array<QLabel*, Settings::NativeAnalog::NumAnalogs> analog_map_modifier_label;
    std::array<QSlider*, Settings::NativeAnalog::NumAnalogs> analog_map_modifier_slider;
    std::array<QGroupBox*, Settings::NativeAnalog::NumAnalogs> analog_map_range_groupbox;
    std::array<QSpinBox*, Settings::NativeAnalog::NumAnalogs> analog_map_range_spinbox;

    static const std::array<std::string, ANALOG_SUB_BUTTONS_NUM> analog_sub_buttons;

    std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>> device_pollers;

    /// A flag to indicate if keyboard keys are okay when configuring an input. If this is false,
    /// keyboard events are ignored.
    bool want_keyboard_mouse = false;

    /// List of physical devices users can map with. If a SDL backed device is selected, then you
    /// can usue this device to get a default mapping.
    std::vector<Common::ParamPackage> input_devices;

    /// Bottom row is where console wide settings are held, and its "owned" by the parent
    /// ConfigureInput widget. On show, add this widget to the main layout. This will change the
    /// parent of the widget to this widget (but thats fine).
    QWidget* bottom_row;
};
