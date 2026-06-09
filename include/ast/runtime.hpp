#pragma once

#include "ast/assembler.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace ast {

struct ChannelSet;

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
