#pragma once

#include "ast/runtime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

namespace ast::detail {

enum class StopReason {
    Halted,
    PcOutOfRange,
    MaxSteps,
    CallDepthExceeded,
};

inline float safe_div(float a, float b) {
    if (std::fabs(b) <= 0.000001F) {
        return 0.0F;
    }
    return a / b;
}

inline std::uint8_t pack_unorm(float value) {
    const float scaled = std::clamp(value, 0.0F, 1.0F) * 255.0F;
    return static_cast<std::uint8_t>(std::lround(scaled));
}

inline std::uint8_t pack_byte(float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 255.0F)));
}

inline float byte_to_unorm(std::uint8_t value) {
    return static_cast<float>(value) / 255.0F;
}

inline int jump_target(const Operand& operand) {
    return static_cast<int>(operand.value);
}

template <typename ProgramT, typename Env>
StopReason execute_program(const ProgramT& program, Env& env, const RunLimits& limits) {
    int pc = 0;
    int steps = 0;
    std::array<int, 32> call_stack{};
    int call_depth = 0;
    const int max_call_depth =
        std::clamp(limits.max_call_depth, 0, static_cast<int>(call_stack.size()));

    while (pc >= 0 && pc < static_cast<int>(program.code.size())) {
        if (steps >= limits.max_steps) {
            return StopReason::MaxSteps;
        }
        ++steps;

        const auto& ins = program.code[static_cast<std::size_t>(pc)];
        const auto& o = ins.operands;
        ++pc;

        auto value = [&](int index) -> float {
            return env.read(o[static_cast<std::size_t>(index)]);
        };
        auto write = [&](int index, float result) {
            env.write(o[static_cast<std::size_t>(index)], result);
        };

        switch (ins.op) {
        case Op::Mov:
            write(0, value(1));
            break;
        case Op::Add:
            write(0, value(1) + value(2));
            break;
        case Op::Sub:
            write(0, value(1) - value(2));
            break;
        case Op::Mul:
            write(0, value(1) * value(2));
            break;
        case Op::Div:
        case Op::Norm:
            write(0, safe_div(value(1), value(2)));
            break;
        case Op::Sin:
            write(0, std::sin(value(1)));
            break;
        case Op::Cos:
            write(0, std::cos(value(1)));
            break;
        case Op::Sqrt:
            write(0, std::sqrt(std::max(0.0F, value(1))));
            break;
        case Op::Abs:
            write(0, std::fabs(value(1)));
            break;
        case Op::Floor:
            write(0, std::floor(value(1)));
            break;
        case Op::Fract:
            write(0, value(1) - std::floor(value(1)));
            break;
        case Op::Min:
            write(0, std::min(value(1), value(2)));
            break;
        case Op::Max:
            write(0, std::max(value(1), value(2)));
            break;
        case Op::Mod:
            write(0, std::fmod(value(1), value(2)));
            break;
        case Op::Lt:
            write(0, value(1) < value(2) ? 1.0F : 0.0F);
            break;
        case Op::Gt:
            write(0, value(1) > value(2) ? 1.0F : 0.0F);
            break;
        case Op::Eq:
            write(0, std::fabs(value(1) - value(2)) <= 0.000001F ? 1.0F : 0.0F);
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
                return StopReason::CallDepthExceeded;
            }
            call_stack[static_cast<std::size_t>(call_depth)] = pc;
            ++call_depth;
            pc = jump_target(o[0]);
            break;
        case Op::Out:
            env.output_unorm(value(0), value(1), value(2), value(3));
            break;
        case Op::Out8:
            env.output_byte(value(0), value(1), value(2), value(3));
            break;
        case Op::Tex: {
            const Rgba sample = env.sample_channel(static_cast<int>(value(4)), value(5), value(6));
            write(0, byte_to_unorm(sample.r));
            write(1, byte_to_unorm(sample.g));
            write(2, byte_to_unorm(sample.b));
            write(3, byte_to_unorm(sample.a));
            break;
        }
        case Op::Texel: {
            const Rgba sample = env.sample_channel_texel(static_cast<int>(value(4)),
                                                         static_cast<int>(std::lround(value(5))),
                                                         static_cast<int>(std::lround(value(6))));
            write(0, byte_to_unorm(sample.r));
            write(1, byte_to_unorm(sample.g));
            write(2, byte_to_unorm(sample.b));
            write(3, byte_to_unorm(sample.a));
            break;
        }
        case Op::Chdim: {
            const auto dimensions = env.channel_dimensions(static_cast<int>(value(2)));
            write(0, static_cast<float>(dimensions.first));
            write(1, static_cast<float>(dimensions.second));
            break;
        }
        case Op::Chtime:
            write(0, env.channel_time(static_cast<int>(value(1))));
            break;
        case Op::Chsrate:
            write(0, env.channel_sample_rate(static_cast<int>(value(1))));
            break;
        case Op::Key:
            write(0, env.key_state(static_cast<int>(value(1))));
            break;
        case Op::Mbtn:
            write(0, env.mouse_button_state(static_cast<int>(value(1))));
            break;
        case Op::Mwheel:
            write(0, env.mouse_wheel_x());
            write(1, env.mouse_wheel_y());
            break;
        case Op::Gbtn:
            write(0, env.gamepad_button_state(static_cast<int>(value(1))));
            break;
        case Op::Gaxis:
            write(0, env.gamepad_axis_state(static_cast<int>(value(1))));
            break;
        case Op::Ret:
            if (call_depth > 0) {
                --call_depth;
                pc = call_stack[static_cast<std::size_t>(call_depth)];
                break;
            }
            return StopReason::Halted;
        case Op::Halt:
            return StopReason::Halted;
        }
    }

    return StopReason::PcOutOfRange;
}

} // namespace ast::detail
