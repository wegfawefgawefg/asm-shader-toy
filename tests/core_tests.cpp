#include "ast/assembler.hpp"
#include "ast/runtime.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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

void test_standard_include_aliases() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .include <std/screen.inc>
        out uv_x, uv_y, pos_x, 1.0
    )");

    require(result.ok(), "standard include aliases assemble");
    const ast::Rgba color =
        ast::run_pixel(result.program, ast::PixelInputs{120, 80, 240, 160, 0.0F});
    require(color.r == 128 && color.g == 128, "standard uv aliases are seeded");
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

void test_builtin_aliases_cannot_be_redefined() {
    const ast::AssembleResult result = ast::assemble_source(".alias time, r16\n");
    require(!result.ok(), "builtin aliases cannot be redefined");
}

void test_standard_aliases_cannot_be_redefined() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .include <std/aliases.inc>
        .alias tmp0, r48
    )");
    require(!result.ok(), "standard aliases cannot be redefined");
}

void test_user_aliases_cannot_preempt_standard_aliases() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .alias tmp0, r48
        .include <std/aliases.inc>
    )");
    require(!result.ok(), "user aliases cannot preempt standard aliases");
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

void test_texel_sampling() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        texel r16, r17, r18, r19, 0, 1, 0
        out r16, r17, r18, r19
    )");

    require(result.ok(), "texel program assembles");

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
            "texel sampler reads exact channel pixels");
}

void test_texel_out_of_bounds_is_black() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        texel r16, r17, r18, r19, 0, -1, 0
        out r16, r17, r18, r19
    )");

    require(result.ok(), "out-of-bounds texel program assembles");

    ast::ChannelSet channels;
    channels.image[0].width = 1;
    channels.image[0].height = 1;
    channels.image[0].pixels = {0xFFFFFFFFU};

    ast::PixelInputs inputs;
    inputs.channels = &channels;
    const ast::Rgba color = ast::run_pixel(result.program, inputs);
    require(color.r == 0 && color.g == 0 && color.b == 0 && color.a == 0,
            "out-of-bounds texel samples transparent black");
}

void test_file_dependencies_include_main_and_includes() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "asm-shader-toy-core-tests";
    std::filesystem::create_directories(dir);
    const std::filesystem::path include_path = dir / "colors.inc";
    const std::filesystem::path main_path = dir / "main.asm";

    {
        std::ofstream include_file(include_path);
        include_file << ".const red 1.0\n";
    }
    {
        std::ofstream main_file(main_path);
        main_file << ".include \"colors.inc\"\n"
                  << "out red, 0.0, 0.0, 1.0\n";
    }

    const ast::AssembleResult result = ast::assemble_file(main_path);
    require(result.ok(), "file with local include assembles");
    require(result.dependencies.size() == 2, "main file and include are dependencies");
    require(result.dependencies[0] == std::filesystem::weakly_canonical(main_path),
            "main file is first dependency");
    require(result.dependencies[1] == std::filesystem::weakly_canonical(include_path),
            "local include is tracked as dependency");
}

void test_includes_are_once_by_default() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "asm-shader-toy-core-tests";
    std::filesystem::create_directories(dir);
    const std::filesystem::path include_path = dir / "once.inc";
    const std::filesystem::path main_path = dir / "include-once.asm";

    {
        std::ofstream include_file(include_path);
        include_file << "only_once:\n"
                     << "out 1.0, 0.0, 0.0, 1.0\n";
    }
    {
        std::ofstream main_file(main_path);
        main_file << ".include \"once.inc\"\n"
                  << ".include \"once.inc\"\n"
                  << "jmp only_once\n";
    }

    const ast::AssembleResult result = ast::assemble_file(main_path);
    require(result.ok(), "including the same file twice is allowed");
    require(result.dependencies.size() == 2, "duplicate include is tracked only once");
}

void test_recursive_include_reports_error() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "asm-shader-toy-core-tests";
    std::filesystem::create_directories(dir);
    const std::filesystem::path include_path = dir / "recursive.inc";
    const std::filesystem::path main_path = dir / "recursive.asm";

    {
        std::ofstream include_file(include_path);
        include_file << ".include \"recursive.inc\"\n";
    }
    {
        std::ofstream main_file(main_path);
        main_file << ".include \"recursive.inc\"\n"
                  << "out 0.0, 0.0, 0.0, 1.0\n";
    }

    const ast::AssembleResult result = ast::assemble_file(main_path);
    require(!result.ok(), "recursive include still reports an error");
}

void test_example_assembles() {
    if (!std::filesystem::exists("examples")) {
        return;
    }

    const char* examples[] = {
        "examples/basics/plasma.asm",
        "examples/basics/time_pulse.asm",
        "examples/buffers/life_buffer.asm",
        "examples/buffers/life_display.asm",
        "examples/input/mouse_rings.asm",
        "examples/multifile/main.asm",
        "examples/textures/image_passthrough.asm",
        "examples/textures/multi_image_mix.asm",
        "examples/video/poster_edges.asm",
        "examples/video/video_channel.asm",
        "examples/webcam/webcam_channel.asm",
        "examples/raymarch/planet_sphere.asm",
        "examples/raymarch/pixelated_planet.asm",
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
    test_standard_include_aliases();
    test_high_register_aliases_are_allowed();
    test_input_aliases_are_read_only();
    test_builtin_aliases_cannot_be_redefined();
    test_standard_aliases_cannot_be_redefined();
    test_user_aliases_cannot_preempt_standard_aliases();
    test_texture_sampling();
    test_texel_sampling();
    test_texel_out_of_bounds_is_black();
    test_file_dependencies_include_main_and_includes();
    test_includes_are_once_by_default();
    test_recursive_include_reports_error();
    test_example_assembles();
    test_diagnostics();
    return 0;
}
