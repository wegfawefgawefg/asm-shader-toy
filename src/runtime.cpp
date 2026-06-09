#include "ast/runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <thread>

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

Rgba unpack_rgba(std::uint32_t pixel) {
    return Rgba{static_cast<std::uint8_t>(pixel & 0xFFU),
                static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU),
                static_cast<std::uint8_t>((pixel >> 16U) & 0xFFU),
                static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU)};
}

Rgba sample_channel(const ChannelSet* channels, int channel_index, float u, float v) {
    if (channels == nullptr || channel_index < 0 ||
        channel_index >= static_cast<int>(channels->image.size())) {
        return Rgba{};
    }

    const ImageChannel& channel = channels->image[static_cast<std::size_t>(channel_index)];
    if (!channel.loaded()) {
        return Rgba{};
    }

    const float clamped_u = std::clamp(u, 0.0F, 1.0F);
    const float clamped_v = std::clamp(v, 0.0F, 1.0F);
    const int x =
        std::clamp(static_cast<int>(std::floor(clamped_u * static_cast<float>(channel.width))), 0,
                   channel.width - 1);
    const int y =
        std::clamp(static_cast<int>(std::floor(clamped_v * static_cast<float>(channel.height))), 0,
                   channel.height - 1);
    return unpack_rgba(channel.pixel_data()[static_cast<std::size_t>(y * channel.width + x)]);
}

Rgba sample_channel_texel(const ChannelSet* channels, int channel_index, int x, int y) {
    if (channels == nullptr || channel_index < 0 ||
        channel_index >= static_cast<int>(channels->image.size())) {
        return Rgba{0, 0, 0, 0};
    }

    const ImageChannel& channel = channels->image[static_cast<std::size_t>(channel_index)];
    if (!channel.loaded() || x < 0 || y < 0 || x >= channel.width || y >= channel.height) {
        return Rgba{0, 0, 0, 0};
    }

    return unpack_rgba(channel.pixel_data()[static_cast<std::size_t>(y * channel.width + x)]);
}

float byte_to_unorm(std::uint8_t value) {
    return static_cast<float>(value) / 255.0F;
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
    registers[8] = inputs.mouse_click_x;
    registers[9] = inputs.mouse_click_y;
    registers[10] = static_cast<float>(inputs.frame);
    registers[11] = inputs.time_delta;
    registers[12] = inputs.wall_clock_seconds;
    registers[13] = static_cast<float>(inputs.year);
    registers[14] = static_cast<float>(inputs.month);
    registers[15] = static_cast<float>(inputs.day);
}

void seed_frame_inputs(Registers& registers, const FrameInputs& inputs) {
    registers.fill(0.0F);
    registers[2] = inputs.time;
    registers[3] = static_cast<float>(inputs.width);
    registers[4] = static_cast<float>(inputs.height);
    registers[5] = inputs.mouse_x;
    registers[6] = inputs.mouse_y;
    registers[7] = inputs.mouse_down;
    registers[8] = inputs.mouse_click_x;
    registers[9] = inputs.mouse_click_y;
    registers[10] = static_cast<float>(inputs.frame);
    registers[11] = inputs.time_delta;
    registers[12] = inputs.wall_clock_seconds;
    registers[13] = static_cast<float>(inputs.year);
    registers[14] = static_cast<float>(inputs.month);
    registers[15] = static_cast<float>(inputs.day);
}

std::uint32_t pack_rgba(Rgba rgba) {
    return (static_cast<std::uint32_t>(rgba.a) << 24U) |
           (static_cast<std::uint32_t>(rgba.b) << 16U) |
           (static_cast<std::uint32_t>(rgba.g) << 8U) | static_cast<std::uint32_t>(rgba.r);
}

Rgba run_pixel_registers(const Program& program, Registers registers, const ChannelSet* channels,
                         const RunLimits& limits) {
    Rgba color{0, 0, 0, 255};
    int pc = 0;
    int steps = 0;
    std::array<int, 32> call_stack{};
    int call_depth = 0;
    const int max_call_depth =
        std::clamp(limits.max_call_depth, 0, static_cast<int>(call_stack.size()));

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
        case Op::Jz:
            if (value(0) == 0.0F) {
                pc = jump_target(o[1]);
            }
            break;
        case Op::Jeq:
            if (std::fabs(value(0) - value(1)) <= 0.000001F) {
                pc = jump_target(o[2]);
            }
            break;
        case Op::Jne:
            if (std::fabs(value(0) - value(1)) > 0.000001F) {
                pc = jump_target(o[2]);
            }
            break;
        case Op::Jlt:
            if (value(0) < value(1)) {
                pc = jump_target(o[2]);
            }
            break;
        case Op::Jle:
            if (value(0) <= value(1)) {
                pc = jump_target(o[2]);
            }
            break;
        case Op::Jgt:
            if (value(0) > value(1)) {
                pc = jump_target(o[2]);
            }
            break;
        case Op::Jge:
            if (value(0) >= value(1)) {
                pc = jump_target(o[2]);
            }
            break;
        case Op::Call:
            if (call_depth >= max_call_depth) {
                return color;
            }
            call_stack[static_cast<std::size_t>(call_depth)] = pc;
            ++call_depth;
            pc = jump_target(o[0]);
            break;
        case Op::Out:
            color = Rgba{pack_unorm(value(0)), pack_unorm(value(1)), pack_unorm(value(2)),
                         pack_unorm(value(3))};
            break;
        case Op::Out8:
            color = Rgba{pack_byte(value(0)), pack_byte(value(1)), pack_byte(value(2)),
                         pack_byte(value(3))};
            break;
        case Op::Tex: {
            const Rgba sample =
                sample_channel(channels, static_cast<int>(value(4)), value(5), value(6));
            dst(0) = byte_to_unorm(sample.r);
            dst(1) = byte_to_unorm(sample.g);
            dst(2) = byte_to_unorm(sample.b);
            dst(3) = byte_to_unorm(sample.a);
            break;
        }
        case Op::Texel: {
            const Rgba sample = sample_channel_texel(channels, static_cast<int>(value(4)),
                                                     static_cast<int>(std::lround(value(5))),
                                                     static_cast<int>(std::lround(value(6))));
            dst(0) = byte_to_unorm(sample.r);
            dst(1) = byte_to_unorm(sample.g);
            dst(2) = byte_to_unorm(sample.b);
            dst(3) = byte_to_unorm(sample.a);
            break;
        }
        case Op::Ret:
            if (call_depth > 0) {
                --call_depth;
                pc = call_stack[static_cast<std::size_t>(call_depth)];
                break;
            }
            return color;
        case Op::Halt:
            return color;
        }
    }

    return color;
}

} // namespace

Rgba run_pixel(const Program& program, const PixelInputs& inputs, const RunLimits& limits) {
    Registers registers{};
    seed_inputs(registers, inputs);
    return run_pixel_registers(program, registers, inputs.channels, limits);
}

void render_frame(const Program& program, const FrameInputs& inputs,
                  std::vector<std::uint32_t>& pixels, const RunLimits& limits) {
    pixels.resize(static_cast<std::size_t>(inputs.width * inputs.height));
    Registers base_registers{};
    seed_frame_inputs(base_registers, inputs);

    const int total_pixels = inputs.width * inputs.height;
    const unsigned int hardware_threads = std::thread::hardware_concurrency();
    const int worker_count =
        total_pixels >= 4096
            ? std::max(1, std::min(inputs.height, static_cast<int>(hardware_threads)))
            : 1;

    auto render_rows = [&](int begin_y, int end_y) {
        for (int y = begin_y; y < end_y; ++y) {
            for (int x = 0; x < inputs.width; ++x) {
                Registers registers = base_registers;
                registers[0] = static_cast<float>(x);
                registers[1] = static_cast<float>(y);
                pixels[static_cast<std::size_t>(y * inputs.width + x)] =
                    pack_rgba(run_pixel_registers(program, registers, inputs.channels, limits));
            }
        }
    };

    if (worker_count == 1) {
        render_rows(0, inputs.height);
        return;
    }

    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(worker_count - 1));
    for (int worker = 1; worker < worker_count; ++worker) {
        const int begin_y = (inputs.height * worker) / worker_count;
        const int end_y = (inputs.height * (worker + 1)) / worker_count;
        workers.emplace_back(render_rows, begin_y, end_y);
    }

    render_rows(0, inputs.height / worker_count);
    for (std::thread& worker : workers) {
        worker.join();
    }
}

} // namespace ast
