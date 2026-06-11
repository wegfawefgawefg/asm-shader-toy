#include "ast/assembler.hpp"
#include "ast/ir.hpp"
#include "ast/runtime.hpp"
#include "ast/wgsl.hpp"

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

void test_const_block_math_exports_constants() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .consts
            mov pi, 3.14159265
            add tau, pi, pi
            mul half_tau, tau, 0.5
            div inv_255, 1.0, 255.0
        .end

        mul r16, half_tau, inv_255
        out half_tau, tau, r16, 1.0
    )");

    require(result.ok(), ".consts math program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.r == 255, ".consts exports half_tau");
    require(color.g == 255, ".consts exports tau");
    require(color.b == 3, ".consts exported constants can combine at runtime");
}

void test_const_block_can_use_previous_constants() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .const base 10
        .consts
            add bigger, base, 5
        .end
        out8 bigger, 0, 0, 255
    )");

    require(result.ok(), ".consts can read previous constants");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.r == 15, ".consts reads previous constants");
}

void test_const_block_branches_local_labels_and_calls() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .consts
        main:
            mov i, 0
            mov acc, 0
        .loop:
            add acc, acc, 0.25
            add i, i, 1
            jlt i, 4, .loop
            call scale
            halt
        scale:
            mul one_twenty_eight, acc, 128
            ret
        .end

        out8 one_twenty_eight, 0, 0, 255
    )");

    require(result.ok(), ".consts control flow program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.r == 128, ".consts supports branches, local labels, and calls");
}

void test_const_block_rejects_runtime_aliases() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .consts
            mov bad, time
        .end
        out8 0, 0, 0, 255
    )");
    require(!result.ok(), ".consts rejects runtime aliases");
}

void test_const_block_rejects_runtime_only_ops() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .consts
            out 1.0, 0.0, 0.0, 1.0
        .end
        out8 0, 0, 0, 255
    )");
    require(!result.ok(), ".consts rejects runtime output ops");
}

void test_const_block_rejects_channel_metadata_ops() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .consts
            chdim w, h, 0
        .end
        out8 0, 0, 0, 255
    )");
    require(!result.ok(), ".consts rejects channel metadata ops");
}

void test_const_block_rejects_live_input_ops() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .consts
            key pressed, 4
        .end
        out8 0, 0, 0, 255
    )");
    require(!result.ok(), ".consts rejects live input ops");
}

void test_const_block_max_steps_reports_error() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        .consts
        loop:
            jmp loop
        .end
        out8 0, 0, 0, 255
    )");
    require(!result.ok(), ".consts max-step failures are diagnostics");
}

void test_subroutine_call_ret_and_halt() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        mov r16, 40
        call add_two
        out8 r16, 0, 0, 255
        halt
    add_two:
        add r16, r16, 2
        ret
    )");

    require(result.ok(), "subroutine program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.r == 42, "call returns to the caller");
}

void test_ret_without_call_still_halts() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        out8 7, 0, 0, 255
        ret
        out8 255, 0, 0, 255
    )");

    require(result.ok(), "legacy ret program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.r == 7, "ret without a call still halts");
}

void test_local_labels_are_scoped_to_global_labels() {
    const ast::AssembleResult result = ast::assemble_source(R"(
    main:
        mov r16, 0
    .loop:
        add r16, r16, 1
        jlt r16, 3, .loop
        call helper
        out8 r16, 0, 0, 255
        halt
    helper:
        add r16, r16, 10
    .loop:
        add r16, r16, 1
        jlt r16, 15, .loop
        ret
    )");

    require(result.ok(), "local label program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.r == 15, "local labels resolve within the current global label");
}

void test_local_label_requires_global_label() {
    const ast::AssembleResult result = ast::assemble_source(R"(
    .loop:
        out8 0, 0, 0, 255
    )");
    require(!result.ok(), "local labels require a preceding global label");
}

void test_branch_helpers() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        mov r16, 1
        jz r16, fail
        jeq r16, 1, ok1
        jmp fail
    ok1:
        jne r16, 2, ok2
        jmp fail
    ok2:
        jle r16, 1, ok3
        jmp fail
    ok3:
        jge r16, 1, ok4
        jmp fail
    ok4:
        jgt 2, r16, ok5
        jmp fail
    ok5:
        jlt r16, 2, pass
        jmp fail
    fail:
        out8 255, 0, 0, 255
        halt
    pass:
        out8 0, 255, 0, 255
        halt
    )");

    require(result.ok(), "branch helper program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.g == 255 && color.r == 0, "branch helpers jump correctly");
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

void test_channel_dimensions_and_time() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        chdim r16, r17, 0
        chtime r18, 0
        chsrate r19, 0
        div r19, r19, 1000
        out8 r16, r17, r18, r19
    )");

    require(result.ok(), "channel metadata program assembles");

    ast::ChannelSet channels;
    channels.image[0].width = 12;
    channels.image[0].height = 34;
    channels.image[0].time = 56.0F;
    channels.image[0].sample_rate = 44100.0F;
    channels.image[0].pixels = {0xFFFFFFFFU};

    ast::PixelInputs inputs;
    inputs.channels = &channels;
    const ast::Rgba color = ast::run_pixel(result.program, inputs);
    require(color.r == 12 && color.g == 34 && color.b == 56 && color.a == 44,
            "channel metadata instructions read dimensions, time, and sample rate");
}

void test_missing_channel_metadata_is_zero() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        chdim r16, r17, 3
        chtime r18, 3
        chsrate r19, 3
        out8 r16, r17, r18, r19
    )");

    require(result.ok(), "missing channel metadata program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.r == 0 && color.g == 0 && color.b == 0 && color.a == 0,
            "missing channel metadata reads as zero");
}

void test_live_input_queries() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        key r16, 4
        mbtn r17, 1
        mwheel r18, r19
        gbtn r20, 0
        gaxis r21, 2
        out8 r16, r17, r18, r19
    )");
    const ast::AssembleResult gamepad_result = ast::assemble_source(R"(
        gbtn r16, 0
        gaxis r17, 2
        out8 r16, r17, 0, 255
    )");

    require(result.ok(), "live input query program assembles");
    require(gamepad_result.ok(), "gamepad input query program assembles");

    ast::InputState input_state;
    input_state.keys[4] = 10.0F;
    input_state.mouse_buttons[1] = 20.0F;
    input_state.mouse_wheel_x = 30.0F;
    input_state.mouse_wheel_y = 40.0F;
    input_state.gamepad_buttons[0] = 50.0F;
    input_state.gamepad_axes[2] = 60.0F;

    ast::PixelInputs inputs;
    inputs.input_state = &input_state;
    const ast::Rgba color = ast::run_pixel(result.program, inputs);
    const ast::Rgba gamepad_color = ast::run_pixel(gamepad_result.program, inputs);
    require(color.r == 10 && color.g == 20 && color.b == 30 && color.a == 40,
            "live input queries read the input snapshot");
    require(gamepad_color.r == 50 && gamepad_color.g == 60,
            "gamepad input queries read the input snapshot");
}

void test_missing_live_input_queries_are_zero() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        key r16, 4
        mbtn r17, 1
        gbtn r18, 0
        gaxis r19, 2
        out8 r16, r17, r18, r19
    )");

    require(result.ok(), "missing live input query program assembles");
    const ast::Rgba color = ast::run_pixel(result.program, ast::PixelInputs{});
    require(color.r == 0 && color.g == 0 && color.b == 0 && color.a == 0,
            "missing live input snapshot reads as zero");
}

void test_ir_lowering_classifies_features_and_successors() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        tex r16, r17, r18, r19, 0, 0.5, 0.5
        key r20, 4
        jnz r20, lit
        out8 0, 0, 0, 255
        halt
    lit:
        chdim r21, r22, 0
        out r16, r17, r18, r19
    )");

    require(result.ok(), "IR feature fixture assembles");
    const ast::LowerIrResult lowered = ast::lower_to_ir(result.program);
    require(lowered.ok(), "IR lowering succeeds");
    require(lowered.program.has_textures, "IR detects texture operations");
    require(lowered.program.has_live_input, "IR detects live input operations");
    require(lowered.program.has_channel_metadata, "IR detects channel metadata operations");
    require(lowered.program.has_control_flow, "IR detects control flow");
    require(lowered.program.has_output, "IR detects output operations");
    require(lowered.program.code[2].successor_count == 2, "conditional branch has two successors");
    require(lowered.program.code[2].successors[0] == 5, "conditional branch target is recorded");
    require(lowered.program.code[2].successors[1] == 3,
            "conditional branch fallthrough is recorded");
    require(lowered.program.code[4].successor_count == 0, "halt has no successor");
}

void test_ir_rejects_invalid_jump_target() {
    ast::Program program;
    ast::Instruction instruction;
    instruction.op = ast::Op::Jmp;
    instruction.operand_count = 1;
    instruction.operands[0] = ast::Operand{ast::OperandKind::Immediate, 0, 99.0F};
    instruction.line = 1;
    program.code.push_back(instruction);

    const ast::LowerIrResult lowered = ast::lower_to_ir(program);
    require(!lowered.ok(), "IR rejects jump targets outside the program");
}

void test_cpu_runs_lowered_ir_program() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        norm r16, px, width
        norm r17, py, height
        add r18, r16, r17
        out r18, r16, r17, 1.0
    )");

    require(result.ok(), "IR runtime parity fixture assembles");
    const ast::LowerIrResult lowered = ast::lower_to_ir(result.program);
    require(lowered.ok(), "IR runtime parity fixture lowers");

    const ast::PixelInputs inputs{60, 80, 240, 160, 0.0F};
    const ast::Rgba from_program = ast::run_pixel(result.program, inputs);
    const ast::Rgba from_ir = ast::run_pixel(lowered.program, inputs);
    require(from_program.r == from_ir.r && from_program.g == from_ir.g &&
                from_program.b == from_ir.b && from_program.a == from_ir.a,
            "CPU Program and IR execution match");
}

void test_wgsl_emits_arithmetic_control_and_output_subset() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        mov r16, 0
    loop:
        add r16, r16, 1
        jlt r16, 3, loop
        out8 r16, 0, 0, 255
        halt
    )");

    require(result.ok(), "WGSL fixture assembles");
    const ast::WgslCompileResult wgsl = ast::compile_wgsl(result.program);
    require(wgsl.ok(), "WGSL emitter accepts arithmetic/control/output subset");
    require(wgsl.source.find("@compute @workgroup_size") != std::string::npos,
            "WGSL source contains compute entrypoint");
    require(wgsl.source.find("switch (pc)") != std::string::npos,
            "WGSL source contains program-counter dispatch");
    require(wgsl.source.find("textureStore") != std::string::npos,
            "WGSL source writes the output texture");
}

void test_wgsl_emits_texture_and_channel_metadata_subset() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        tex r16, r17, r18, r19, 0, 0.5, 0.5
        texel r20, r21, r22, r23, 1, 4, 5
        chdim r24, r25, 0
        chtime r26, 0
        chsrate r27, 0
        out r16, r21, r26, r19
    )");

    require(result.ok(), "WGSL texture fixture assembles");
    const ast::WgslCompileResult wgsl = ast::compile_wgsl(result.program);
    require(wgsl.ok(), "WGSL emitter accepts texture/channel metadata subset");
    require(wgsl.source.find("@group(0) @binding(2) var channel0_texture") != std::string::npos,
            "WGSL source declares channel texture bindings");
    require(wgsl.source.find("fn ast_channel_meta") != std::string::npos,
            "WGSL source declares channel metadata helper");
    require(wgsl.source.find("textureLoad(channel0_texture") != std::string::npos,
            "WGSL source samples channel textures");
    require(wgsl.source.find("let tex_channel_0 = 0;") != std::string::npos,
            "WGSL source emits immediate texture channels as integer literals");
    require(wgsl.source.find("ast_channel_meta(0);") != std::string::npos,
            "WGSL source emits immediate metadata channels as integer literals");
}

void test_wgsl_emits_live_input_subset() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        key r16, 4
        mbtn r17, 1
        mwheel r18, r19
        gbtn r20, 0
        gaxis r21, 2
        out8 r16, r17, r18, r19
    )");

    require(result.ok(), "WGSL live input fixture assembles");
    const ast::WgslCompileResult wgsl = ast::compile_wgsl(result.program);
    require(wgsl.ok(), "WGSL emitter accepts live input subset");
    require(wgsl.source.find("keys: array<vec4<f32>, 128>") != std::string::npos,
            "WGSL source declares packed key state");
    require(wgsl.source.find("fn ast_key_state") != std::string::npos,
            "WGSL source declares key helper");
    require(wgsl.source.find("ast_mouse_button_state") != std::string::npos,
            "WGSL source declares mouse button helper");
    require(wgsl.source.find("ast_gamepad_axis_state") != std::string::npos,
            "WGSL source declares gamepad axis helper");
}

void test_wgsl_emits_call_and_ret_subset() {
    const ast::AssembleResult result = ast::assemble_source(R"(
        call helper
        out8 r16, 0, 0, 255
        halt
    helper:
        mov r16, 255
        ret
    )");

    require(result.ok(), "WGSL call fixture assembles");
    const ast::WgslCompileResult wgsl = ast::compile_wgsl(result.program);
    require(wgsl.ok(), "WGSL emitter accepts call/ret subset");
    require(wgsl.source.find("var call_stack: array<i32, 32>") != std::string::npos,
            "WGSL source declares call stack");
    require(wgsl.source.find("call_stack[call_depth]") != std::string::npos,
            "WGSL source pushes return addresses");
    require(wgsl.source.find("pc = call_stack[call_depth]") != std::string::npos,
            "WGSL source pops return addresses");
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
        "examples/basics/consts.asm",
        "examples/basics/plasma.asm",
        "examples/basics/subroutines.asm",
        "examples/basics/time_pulse.asm",
        "examples/audio/audio_scope.asm",
        "examples/buffers/life_buffer.asm",
        "examples/buffers/life_display.asm",
        "examples/input/live_controls.asm",
        "examples/microphone/mic_scope.asm",
        "examples/input/mouse_rings.asm",
        "examples/multifile/main.asm",
        "examples/textures/image_passthrough.asm",
        "examples/textures/multi_image_mix.asm",
        "examples/textures/noise_field.asm",
        "examples/video/channel_metadata.asm",
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
    test_const_block_math_exports_constants();
    test_const_block_can_use_previous_constants();
    test_const_block_branches_local_labels_and_calls();
    test_const_block_rejects_runtime_aliases();
    test_const_block_rejects_runtime_only_ops();
    test_const_block_rejects_channel_metadata_ops();
    test_const_block_rejects_live_input_ops();
    test_const_block_max_steps_reports_error();
    test_subroutine_call_ret_and_halt();
    test_ret_without_call_still_halts();
    test_local_labels_are_scoped_to_global_labels();
    test_local_label_requires_global_label();
    test_branch_helpers();
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
    test_channel_dimensions_and_time();
    test_missing_channel_metadata_is_zero();
    test_live_input_queries();
    test_missing_live_input_queries_are_zero();
    test_ir_lowering_classifies_features_and_successors();
    test_ir_rejects_invalid_jump_target();
    test_cpu_runs_lowered_ir_program();
    test_wgsl_emits_arithmetic_control_and_output_subset();
    test_wgsl_emits_texture_and_channel_metadata_subset();
    test_wgsl_emits_live_input_subset();
    test_wgsl_emits_call_and_ret_subset();
    test_file_dependencies_include_main_and_includes();
    test_includes_are_once_by_default();
    test_recursive_include_reports_error();
    test_example_assembles();
    test_diagnostics();
    return 0;
}
