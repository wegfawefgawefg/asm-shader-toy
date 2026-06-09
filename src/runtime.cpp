#include "ast/runtime.hpp"

#include <algorithm>
#include <cmath>

namespace ast {
namespace {

float read_operand(const Operand& operand, const Registers& registers) {
    if (operand.kind == OperandKind::Register) {
        return registers[static_cast<std::size_t>(operand.reg)];
    }
    return operand.value;
}

int jump_target(const Operand& operand) {
    return static_cast<int>(operand.value);
}

std::uint8_t pack_unorm(float value) {
    const float scaled = std::clamp(value, 0.0F, 1.0F) * 255.0F;
    return static_cast<std::uint8_t>(std::lround(scaled));
}

std::uint8_t pack_byte(float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 255.0F)));
}

float safe_div(float a, float b) {
    if (std::fabs(b) <= 0.000001F) {
        return 0.0F;
    }
    return a / b;
}

void seed_inputs(Registers& registers, const PixelInputs& inputs) {
    registers.fill(0.0F);
    registers[0] = static_cast<float>(inputs.x);
    registers[1] = static_cast<float>(inputs.y);
    registers[2] = inputs.time;
    registers[3] = static_cast<float>(inputs.width);
    registers[4] = static_cast<float>(inputs.height);
    registers[5] = inputs.mouse_x;
    registers[6] = inputs.mouse_y;
    registers[7] = inputs.mouse_down;
}

std::uint32_t pack_rgba(Rgba rgba) {
    return (static_cast<std::uint32_t>(rgba.a) << 24U) |
           (static_cast<std::uint32_t>(rgba.b) << 16U) |
           (static_cast<std::uint32_t>(rgba.g) << 8U) | static_cast<std::uint32_t>(rgba.r);
}

} // namespace

Rgba run_pixel(const Program& program, const PixelInputs& inputs, const RunLimits& limits) {
    Registers registers{};
    seed_inputs(registers, inputs);

    Rgba color{0, 0, 0, 255};
    int pc = 0;
    int steps = 0;

    while (pc >= 0 && pc < static_cast<int>(program.code.size()) && steps < limits.max_steps) {
        ++steps;
        const Instruction& ins = program.code[static_cast<std::size_t>(pc)];
        const auto& o = ins.operands;
        ++pc;

        auto dst = [&](int index) -> float& {
            return registers[static_cast<std::size_t>(o[static_cast<std::size_t>(index)].reg)];
        };
        auto value = [&](int index) -> float {
            return read_operand(o[static_cast<std::size_t>(index)], registers);
        };

        switch (ins.op) {
        case Op::Mov:
            dst(0) = value(1);
            break;
        case Op::Add:
            dst(0) = value(1) + value(2);
            break;
        case Op::Sub:
            dst(0) = value(1) - value(2);
            break;
        case Op::Mul:
            dst(0) = value(1) * value(2);
            break;
        case Op::Div:
        case Op::Norm:
            dst(0) = safe_div(value(1), value(2));
            break;
        case Op::Sin:
            dst(0) = std::sin(value(1));
            break;
        case Op::Cos:
            dst(0) = std::cos(value(1));
            break;
        case Op::Sqrt:
            dst(0) = std::sqrt(std::max(0.0F, value(1)));
            break;
        case Op::Abs:
            dst(0) = std::fabs(value(1));
            break;
        case Op::Floor:
            dst(0) = std::floor(value(1));
            break;
        case Op::Fract:
            dst(0) = value(1) - std::floor(value(1));
            break;
        case Op::Min:
            dst(0) = std::min(value(1), value(2));
            break;
        case Op::Max:
            dst(0) = std::max(value(1), value(2));
            break;
        case Op::Mod:
            dst(0) = std::fmod(value(1), value(2));
            break;
        case Op::Lt:
            dst(0) = value(1) < value(2) ? 1.0F : 0.0F;
            break;
        case Op::Gt:
            dst(0) = value(1) > value(2) ? 1.0F : 0.0F;
            break;
        case Op::Eq:
            dst(0) = std::fabs(value(1) - value(2)) <= 0.000001F ? 1.0F : 0.0F;
            break;
        case Op::Jmp:
            pc = jump_target(o[0]);
            break;
        case Op::Jnz:
            if (value(0) != 0.0F) {
                pc = jump_target(o[1]);
            }
            break;
        case Op::Out:
            color = Rgba{pack_unorm(value(0)), pack_unorm(value(1)), pack_unorm(value(2)),
                         pack_unorm(value(3))};
            break;
        case Op::Out8:
            color = Rgba{pack_byte(value(0)), pack_byte(value(1)), pack_byte(value(2)),
                         pack_byte(value(3))};
            break;
        case Op::Ret:
            return color;
        }
    }

    return color;
}

void render_frame(const Program& program, int width, int height, float time,
                  std::vector<std::uint32_t>& pixels, const RunLimits& limits) {
    pixels.resize(static_cast<std::size_t>(width * height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const PixelInputs inputs{x, y, width, height, time};
            pixels[static_cast<std::size_t>(y * width + x)] =
                pack_rgba(run_pixel(program, inputs, limits));
        }
    }
}

} // namespace ast
