// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include "common/math_util.h"
#include "core/settings.h"
#include "input_common/analog_from_button.h"

namespace InputCommon {

class Analog final : public Input::AnalogDevice {
public:
    using Button = std::unique_ptr<Input::ButtonDevice>;

    Analog(Button up_, Button down_, Button left_, Button right_, Button modifier_,
           float modifier_scale_, float modifier_angle_)
        : up(std::move(up_)), down(std::move(down_)), left(std::move(left_)),
          right(std::move(right_)), modifier(std::move(modifier_)), modifier_scale(modifier_scale_),
          modifier_angle(modifier_angle_) {
        update_thread_running.store(true);
        update_thread = std::thread(&Analog::UpdateStatus, this);
    }

    ~Analog() override {
        if (update_thread_running.load()) {
            update_thread_running.store(false);
            if (update_thread.joinable()) {
                update_thread.join();
            }
        }
    }

    void MoveToDirection(bool enable, float to_angle) {
        if (!enable) {
            return;
        }
        constexpr float TAU = Common::PI * 2.0f;
        // Use wider angle to ease the transition.
        constexpr float aperture = TAU * 0.15f;
        const float top_limit = to_angle + aperture;
        const float bottom_limit = to_angle - aperture;

        if ((angle > to_angle && angle <= top_limit) ||
            (angle + TAU > to_angle && angle + TAU <= top_limit)) {
            angle -= modifier_angle;
            if (angle < 0) {
                angle += TAU;
            }
        } else if ((angle >= bottom_limit && angle < to_angle) ||
                   (angle - TAU >= bottom_limit && angle - TAU < to_angle)) {
            angle += modifier_angle;
            if (angle >= TAU) {
                angle -= TAU;
            }
        } else {
            angle = to_angle;
        }
    }

    void UpdateStatus() {
        while (update_thread_running.load()) {
            const float coef = modifier->GetStatus() ? modifier_scale : 1.0f;

            bool r = right->GetStatus();
            bool l = left->GetStatus();
            bool u = up->GetStatus();
            bool d = down->GetStatus();

            // Eliminate contradictory movements
            if (r && l) {
                r = false;
                l = false;
            }
            if (u && d) {
                u = false;
                d = false;
            }

            // Move to the right
            MoveToDirection(r && !u && !d, 0.0f);

            // Move to the upper right
            MoveToDirection(r && u && !d, Common::PI * 0.25f);

            // Move up
            MoveToDirection(u && !l && !r, Common::PI * 0.5f);

            // Move to the upper left
            MoveToDirection(l && u && !d, Common::PI * 0.75f);

            // Move to the left
            MoveToDirection(l && !u && !d, Common::PI);

            // Move to the bottom left
            MoveToDirection(l && !u && d, Common::PI * 1.25f);

            // Move down
            MoveToDirection(d && !l && !r, Common::PI * 1.5f);

            // Move to the bottom right
            MoveToDirection(r && !u && d, Common::PI * 1.75f);

            // Move if a key is pressed
            if (r || l || u || d) {
                amplitude = coef;
            } else {
                amplitude = 0;
            }

            // Delay the update rate to 100hz
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::tuple<float, float> GetStatus() const override {
        if (Settings::values.emulate_analog_keyboard) {
            return std::make_tuple(std::cos(angle) * amplitude, std::sin(angle) * amplitude);
        }
        constexpr float SQRT_HALF = 0.707106781f;
        int x = 0, y = 0;
        if (right->GetStatus()) {
            ++x;
        }
        if (left->GetStatus()) {
            --x;
        }
        if (up->GetStatus()) {
            ++y;
        }
        if (down->GetStatus()) {
            --y;
        }
        const float coef = modifier->GetStatus() ? modifier_scale : 1.0f;
        return std::make_tuple(static_cast<float>(x) * coef * (y == 0 ? 1.0f : SQRT_HALF),
                               static_cast<float>(y) * coef * (x == 0 ? 1.0f : SQRT_HALF));
    }

    Input::AnalogProperties GetAnalogProperties() const override {
        return {modifier_scale, 1.0f, 0.5f};
    }

    bool GetAnalogDirectionStatus(Input::AnalogDirection direction) const override {
        switch (direction) {
        case Input::AnalogDirection::RIGHT:
            return right->GetStatus();
        case Input::AnalogDirection::LEFT:
            return left->GetStatus();
        case Input::AnalogDirection::UP:
            return up->GetStatus();
        case Input::AnalogDirection::DOWN:
            return down->GetStatus();
        }
        return false;
    }

private:
    Button up;
    Button down;
    Button left;
    Button right;
    Button modifier;
    float modifier_scale;
    float modifier_angle;
    float angle{};
    float amplitude{};
    std::thread update_thread;
    std::atomic<bool> update_thread_running{};
};

std::unique_ptr<Input::AnalogDevice> AnalogFromButton::Create(const Common::ParamPackage& params) {
    const std::string null_engine = Common::ParamPackage{{"engine", "null"}}.Serialize();
    auto up = Input::CreateDevice<Input::ButtonDevice>(params.Get("up", null_engine));
    auto down = Input::CreateDevice<Input::ButtonDevice>(params.Get("down", null_engine));
    auto left = Input::CreateDevice<Input::ButtonDevice>(params.Get("left", null_engine));
    auto right = Input::CreateDevice<Input::ButtonDevice>(params.Get("right", null_engine));
    auto modifier = Input::CreateDevice<Input::ButtonDevice>(params.Get("modifier", null_engine));
    auto modifier_scale = params.Get("modifier_scale", 0.5f);
    auto modifier_angle = params.Get("modifier_angle", 0.035f);
    return std::make_unique<Analog>(std::move(up), std::move(down), std::move(left),
                                    std::move(right), std::move(modifier), modifier_scale,
                                    modifier_angle);
}

} // namespace InputCommon
