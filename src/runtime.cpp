#include "ast/runtime.hpp"

#include "ast/vm_executor.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

namespace ast {
namespace {

Rgba unpack_rgba(std::uint32_t pixel) {
    return Rgba{static_cast<std::uint8_t>(pixel & 0xFFU),
                static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU),
                static_cast<std::uint8_t>((pixel >> 16U) & 0xFFU),
                static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU)};
}

Rgba sample_channel_image(const ChannelSet* channels, int channel_index, float u, float v) {
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

Rgba sample_channel_texel_image(const ChannelSet* channels, int channel_index, int x, int y) {
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

struct RuntimeEnv {
    Registers& registers;
    const ChannelSet* channels = nullptr;
    const InputState* input_state = nullptr;
    Rgba color{0, 0, 0, 255};

    [[nodiscard]] float read(const Operand& operand) const {
        if (operand.kind == OperandKind::Register) {
            return registers[static_cast<std::size_t>(operand.reg)];
        }
        return operand.value;
    }

    void write(const Operand& operand, float value) {
        registers[static_cast<std::size_t>(operand.reg)] = value;
    }

    void output_unorm(float r, float g, float b, float a) {
        color = Rgba{detail::pack_unorm(r), detail::pack_unorm(g), detail::pack_unorm(b),
                     detail::pack_unorm(a)};
    }

    void output_byte(float r, float g, float b, float a) {
        color = Rgba{detail::pack_byte(r), detail::pack_byte(g), detail::pack_byte(b),
                     detail::pack_byte(a)};
    }

    [[nodiscard]] Rgba sample_channel(int channel_index, float u, float v) const {
        return sample_channel_image(channels, channel_index, u, v);
    }

    [[nodiscard]] Rgba sample_channel_texel(int channel_index, int x, int y) const {
        return sample_channel_texel_image(channels, channel_index, x, y);
    }

    [[nodiscard]] std::pair<int, int> channel_dimensions(int channel_index) const {
        if (channels == nullptr || channel_index < 0 ||
            channel_index >= static_cast<int>(channels->image.size())) {
            return {0, 0};
        }
        const ImageChannel& channel = channels->image[static_cast<std::size_t>(channel_index)];
        if (!channel.loaded()) {
            return {0, 0};
        }
        return {channel.width, channel.height};
    }

    [[nodiscard]] float channel_time(int channel_index) const {
        if (channels == nullptr || channel_index < 0 ||
            channel_index >= static_cast<int>(channels->image.size())) {
            return 0.0F;
        }
        return channels->image[static_cast<std::size_t>(channel_index)].time;
    }

    [[nodiscard]] float key_state(int scancode) const {
        if (input_state == nullptr || scancode < 0 || scancode >= key_input_count) {
            return 0.0F;
        }
        return input_state->keys[static_cast<std::size_t>(scancode)];
    }

    [[nodiscard]] float mouse_button_state(int button) const {
        if (input_state == nullptr || button < 0 || button >= mouse_button_input_count) {
            return 0.0F;
        }
        return input_state->mouse_buttons[static_cast<std::size_t>(button)];
    }

    [[nodiscard]] float mouse_wheel_x() const {
        return input_state != nullptr ? input_state->mouse_wheel_x : 0.0F;
    }

    [[nodiscard]] float mouse_wheel_y() const {
        return input_state != nullptr ? input_state->mouse_wheel_y : 0.0F;
    }

    [[nodiscard]] float gamepad_button_state(int button) const {
        if (input_state == nullptr || button < 0 || button >= gamepad_button_input_count) {
            return 0.0F;
        }
        return input_state->gamepad_buttons[static_cast<std::size_t>(button)];
    }

    [[nodiscard]] float gamepad_axis_state(int axis) const {
        if (input_state == nullptr || axis < 0 || axis >= gamepad_axis_input_count) {
            return 0.0F;
        }
        return input_state->gamepad_axes[static_cast<std::size_t>(axis)];
    }
};

Rgba run_pixel_registers(const Program& program, Registers registers, const ChannelSet* channels,
                         const InputState* input_state, const RunLimits& limits) {
    RuntimeEnv env{registers, channels, input_state};
    detail::execute_program(program, env, limits);
    return env.color;
}

} // namespace

Rgba run_pixel(const Program& program, const PixelInputs& inputs, const RunLimits& limits) {
    Registers registers{};
    seed_inputs(registers, inputs);
    return run_pixel_registers(program, registers, inputs.channels, inputs.input_state, limits);
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
                    pack_rgba(run_pixel_registers(program, registers, inputs.channels,
                                                  inputs.input_state, limits));
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
