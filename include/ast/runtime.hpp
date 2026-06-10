#pragma once

#include "ast/assembler.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace ast {

struct ChannelSet;

constexpr int key_input_count = 512;
constexpr int mouse_button_input_count = 8;
constexpr int gamepad_button_input_count = 32;
constexpr int gamepad_axis_input_count = 16;

struct InputState {
    std::array<float, key_input_count> keys{};
    std::array<float, mouse_button_input_count> mouse_buttons{};
    float mouse_wheel_x = 0.0F;
    float mouse_wheel_y = 0.0F;
    std::array<float, gamepad_button_input_count> gamepad_buttons{};
    std::array<float, gamepad_axis_input_count> gamepad_axes{};
};

struct PixelInputs {
    int x = 0;
    int y = 0;
    int width = 240;
    int height = 160;
    float time = 0.0F;
    float time_delta = 0.0F;
    int frame = 0;
    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    float mouse_down = 0.0F;
    float mouse_click_x = 0.0F;
    float mouse_click_y = 0.0F;
    float wall_clock_seconds = 0.0F;
    int year = 1970;
    int month = 1;
    int day = 1;
    const ChannelSet* channels = nullptr;
    const InputState* input_state = nullptr;
};

struct FrameInputs {
    int width = 240;
    int height = 160;
    float time = 0.0F;
    float time_delta = 0.0F;
    int frame = 0;
    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    float mouse_down = 0.0F;
    float mouse_click_x = 0.0F;
    float mouse_click_y = 0.0F;
    float wall_clock_seconds = 0.0F;
    int year = 1970;
    int month = 1;
    int day = 1;
    const ChannelSet* channels = nullptr;
    const InputState* input_state = nullptr;
};

struct RunLimits {
    int max_steps = 4096;
    int max_call_depth = 32;
};

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

struct ImageChannel {
    int width = 0;
    int height = 0;
    float time = 0.0F;
    float sample_rate = 0.0F;
    std::vector<std::uint32_t> pixels;
    const std::vector<std::uint32_t>* external_pixels = nullptr;

    [[nodiscard]] bool loaded() const {
        const std::vector<std::uint32_t>* source =
            external_pixels != nullptr ? external_pixels : &pixels;
        return width > 0 && height > 0 && source != nullptr && !source->empty();
    }

    [[nodiscard]] const std::vector<std::uint32_t>& pixel_data() const {
        return external_pixels != nullptr ? *external_pixels : pixels;
    }
};

struct ChannelSet {
    std::array<ImageChannel, 4> image;
};

using Registers = std::array<float, register_count>;

Rgba run_pixel(const Program& program, const PixelInputs& inputs, const RunLimits& limits = {});
void render_frame(const Program& program, const FrameInputs& inputs,
                  std::vector<std::uint32_t>& pixels, const RunLimits& limits = {});

} // namespace ast
