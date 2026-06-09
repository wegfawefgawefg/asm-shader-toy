#include "ast/assembler.hpp"
#include "ast/runtime.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "test failed: " << message << '\n';
        std::exit(1);
    }
}

void test_basic_output() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        norm r8, r0, r3
        norm r9, r1, r4
        out r8, r9, 0.5, 1.0
    )");

    require(result.ok(), "basic program assembles");
    const ast::Rgba color =
        ast::run_pixel(result.program, ast::PixelInputs{120, 80, 240, 160, 0.0F});
    require(color.r == 128, "x channel is normalized");
    require(color.g == 128, "y channel is normalized");
    require(color.b == 128, "immediate channel is normalized");
    require(color.a == 255, "alpha channel is normalized");
}

void test_labels_and_constants() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .const limit 10
        lt r8, r0, limit
        jnz r8, bright
        out8 0, 0, 0, 255
        ret
    bright:
        out8 255, 32, 16, 255
    )");

    require(result.ok(), "label program assembles");
    const ast::Rgba dark = ast::run_pixel(result.program, ast::PixelInputs{12, 0, 240, 160, 0.0F});
    const ast::Rgba bright = ast::run_pixel(result.program, ast::PixelInputs{5, 0, 240, 160, 0.0F});
    require(dark.r == 0, "false branch runs");
    require(bright.r == 255 && bright.g == 32 && bright.b == 16, "true branch runs");
}

void test_unary_math_uses_destination_and_source() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        sin r8, r2
        cos r9, r8
        sqrt r10, 0.25
        out r10, r9, r8, 1.0
    )");

    require(result.ok(), "unary math program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{0, 0, 240, 160, 0.0F});
    require(color.r == 128, "sqrt source operand is used");
}

void test_frame_inputs_seed_registers() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        out8 r10, r11, r12, r7
    )");

    require(result.ok(), "frame input program assembles");

    ast::PixelInputs inputs;
    inputs.frame = 7;
    inputs.time_delta = 16.0F;
    inputs.wall_clock_seconds = 42.0F;
    inputs.mouse_down = 255.0F;

    const ast::Rgba color = ast::run_pixel(result.program, inputs);
    require(color.r == 7, "frame register is seeded");
    require(color.g == 16, "delta register is seeded");
    require(color.b == 42, "wall-clock register is seeded");
    require(color.a == 255, "mouse-down register is seeded");
}

void test_named_inputs_and_aliases() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .alias u, r16
        .alias v, r17
        norm u, px, width
        norm v, py, height
        out u, v, mouse_down, 1.0
    )");

    require(result.ok(), "named input program assembles");
    const ast::Rgba color =
        ast::run_pixel(result.program, ast::PixelInputs{120, 80, 240, 160, 0.0F});
    require(color.r == 128, "named px/width aliases work");
    require(color.g == 128, "named py/height aliases work");
}

void test_high_register_aliases_are_allowed() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .alias hi, r63
        mov hi, 1.0
        out hi, hi, hi, hi
    )");

    require(result.ok(), "r63 can be named as scratch");
}

void test_input_aliases_are_read_only() {
    const ast::AssembleResult result = ast::assemble_source("mov time, 1.0\n");
    require(!result.ok(), "input aliases cannot be destinations");
}

void test_texture_sampling() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        tex r16, r17, r18, r19, 0, 0.75, 0.25
        out r16, r17, r18, r19
    )");

    require(result.ok(), "texture program assembles");

    ast::ChannelSet channels;
    channels.image[0].width = 2;
    channels.image[0].height = 2;
    channels.image[0].pixels = {
        0xFF0000FFU,
        0xFF00FF00U,
        0xFFFF0000U,
        0xFFFFFFFFU,
    };

    ast::PixelInputs inputs;
    inputs.channels = &channels;
    const ast::Rgba color = ast::run_pixel(result.program, inputs);
    require(color.r == 0 && color.g == 255 && color.b == 0 && color.a == 255,
            "texture sampler reads channel pixels");
}

void test_example_assembles() {
    if (!std::filesystem::exists("examples")) {
        return;
    }

    const char* examples[] = {
        "examples/plasma.asm",          "examples/time_pulse.asm",
        "examples/mouse_rings.asm",     "examples/image_passthrough.asm",
        "examples/multi_image_mix.asm", "examples/planet_sphere.asm",
    };

    for (const char* path : examples) {
        const ast::AssembleResult result = ast::assemble_file(path);
        require(result.ok(), std::string{path} + " assembles");
    }
}

void test_diagnostics() {
    const ast::AssembleResult result = ast::assemble_source("add 4, r0, r1\nwat r0\n");
    require(!result.ok(), "bad program reports diagnostics");
    require(result.diagnostics.size() == 2, "bad program reports two diagnostics");
}

} // namespace

int main() {
    test_basic_output();
    test_labels_and_constants();
    test_unary_math_uses_destination_and_source();
    test_frame_inputs_seed_registers();
    test_named_inputs_and_aliases();
    test_high_register_aliases_are_allowed();
    test_input_aliases_are_read_only();
    test_texture_sampling();
    test_example_assembles();
    test_diagnostics();
    return 0;
}
