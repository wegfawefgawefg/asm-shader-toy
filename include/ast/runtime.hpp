#pragma once

#include "ast/assembler.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace ast {

struct PixelInputs {
    int x = 0;
    int y = 0;
    int width = 240;
    int height = 160;
    float time = 0.0F;
    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    float mouse_down = 0.0F;
};

struct RunLimits {
    int max_steps = 4096;
};

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

using Registers = std::array<float, register_count>;

Rgba run_pixel(const Program& program, const PixelInputs& inputs, const RunLimits& limits = {});
void render_frame(const Program& program, int width, int height, float time,
                  std::vector<std::uint32_t>& pixels, const RunLimits& limits = {});

} // namespace ast
