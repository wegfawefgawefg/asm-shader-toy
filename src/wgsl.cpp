#include "ast/wgsl.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace ast {
namespace {

void add_diag(std::vector<Diagnostic>& diagnostics, const IrInstruction& instruction,
              std::string message) {
    diagnostics.push_back(Diagnostic{instruction.file, instruction.line, std::move(message)});
}

std::string op_name(Op op) {
    switch (op) {
    case Op::Mov:
        return "mov";
    case Op::Add:
        return "add";
    case Op::Sub:
        return "sub";
    case Op::Mul:
        return "mul";
    case Op::Div:
        return "div";
    case Op::Sin:
        return "sin";
    case Op::Cos:
        return "cos";
    case Op::Sqrt:
        return "sqrt";
    case Op::Abs:
        return "abs";
    case Op::Floor:
        return "floor";
    case Op::Fract:
        return "fract";
    case Op::Min:
        return "min";
    case Op::Max:
        return "max";
    case Op::Mod:
        return "mod";
    case Op::Norm:
        return "norm";
    case Op::Lt:
        return "lt";
    case Op::Gt:
        return "gt";
    case Op::Eq:
        return "eq";
    case Op::Jmp:
        return "jmp";
    case Op::Jnz:
        return "jnz";
    case Op::Jz:
        return "jz";
    case Op::Jeq:
        return "jeq";
    case Op::Jne:
        return "jne";
    case Op::Jlt:
        return "jlt";
    case Op::Jle:
        return "jle";
    case Op::Jgt:
        return "jgt";
    case Op::Jge:
        return "jge";
    case Op::Call:
        return "call";
    case Op::Out:
        return "out";
    case Op::Out8:
        return "out8";
    case Op::Tex:
        return "tex";
    case Op::Texel:
        return "texel";
    case Op::Chdim:
        return "chdim";
    case Op::Chtime:
        return "chtime";
    case Op::Chsrate:
        return "chsrate";
    case Op::Key:
        return "key";
    case Op::Mbtn:
        return "mbtn";
    case Op::Mwheel:
        return "mwheel";
    case Op::Gbtn:
        return "gbtn";
    case Op::Gaxis:
        return "gaxis";
    case Op::Ret:
        return "ret";
    case Op::Halt:
        return "halt";
    }
    return "unknown";
}

bool supported_op(Op op) {
    switch (op) {
    case Op::Mov:
    case Op::Add:
    case Op::Sub:
    case Op::Mul:
    case Op::Div:
    case Op::Sin:
    case Op::Cos:
    case Op::Sqrt:
    case Op::Abs:
    case Op::Floor:
    case Op::Fract:
    case Op::Min:
    case Op::Max:
    case Op::Mod:
    case Op::Norm:
    case Op::Lt:
    case Op::Gt:
    case Op::Eq:
    case Op::Jmp:
    case Op::Jnz:
    case Op::Jz:
    case Op::Jeq:
    case Op::Jne:
    case Op::Jlt:
    case Op::Jle:
    case Op::Jgt:
    case Op::Jge:
    case Op::Out:
    case Op::Out8:
    case Op::Tex:
    case Op::Texel:
    case Op::Chdim:
    case Op::Chtime:
    case Op::Chsrate:
    case Op::Key:
    case Op::Mbtn:
    case Op::Mwheel:
    case Op::Gbtn:
    case Op::Gaxis:
    case Op::Call:
    case Op::Ret:
    case Op::Halt:
        return true;
    }
    return false;
}

std::string wgsl_float(float value) {
    if (!std::isfinite(value)) {
        return "0.0";
    }
    std::ostringstream out;
    out << std::setprecision(9) << value;
    std::string text = out.str();
    if (text.find_first_of(".eE") == std::string::npos) {
        text += ".0";
    }
    return text;
}

std::string read_operand(const Operand& operand) {
    if (operand.kind == OperandKind::Register) {
        return "r[" + std::to_string(operand.reg) + "]";
    }
    return wgsl_float(operand.value);
}

std::string read_i32_operand(const Operand& operand) {
    if (operand.kind == OperandKind::Register) {
        return "i32(" + read_operand(operand) + ")";
    }
    return std::to_string(static_cast<int>(operand.value));
}

std::string write_register(const Operand& operand, std::string_view expression) {
    return "r[" + std::to_string(operand.reg) + "] = " + std::string{expression} + ";";
}

int target_operand(const Operand& operand) {
    return static_cast<int>(operand.value);
}

std::string next_pc(int pc) {
    return std::to_string(pc + 1);
}

void emit_header(std::ostringstream& out, const WgslOptions& options) {
    out << "struct AstInputs {\n"
           "    time: f32,\n"
           "    time_delta: f32,\n"
           "    frame: f32,\n"
           "    width: f32,\n"
           "    height: f32,\n"
           "    mouse_x: f32,\n"
           "    mouse_y: f32,\n"
           "    mouse_down: f32,\n"
           "    mouse_click_x: f32,\n"
           "    mouse_click_y: f32,\n"
           "    wall_clock_seconds: f32,\n"
           "    year: f32,\n"
           "    month: f32,\n"
           "    day: f32,\n"
           "    channel0: vec4<f32>,\n"
           "    channel1: vec4<f32>,\n"
           "    channel2: vec4<f32>,\n"
           "    channel3: vec4<f32>,\n"
           "    keys: array<vec4<f32>, 128>,\n"
           "    mouse_buttons: array<vec4<f32>, 2>,\n"
           "    mouse_wheel: vec4<f32>,\n"
           "    gamepad_buttons: array<vec4<f32>, 8>,\n"
           "    gamepad_axes: array<vec4<f32>, 4>,\n"
           "};\n\n"
           "@group(0) @binding(0) var output_texture: texture_storage_2d<rgba8unorm, write>;\n"
           "@group(0) @binding(1) var<uniform> ast_inputs: AstInputs;\n\n"
           "@group(0) @binding(2) var channel0_texture: texture_2d<f32>;\n"
           "@group(0) @binding(3) var channel1_texture: texture_2d<f32>;\n"
           "@group(0) @binding(4) var channel2_texture: texture_2d<f32>;\n"
           "@group(0) @binding(5) var channel3_texture: texture_2d<f32>;\n\n"
           "fn ast_safe_div(a: f32, b: f32) -> f32 {\n"
           "    if (abs(b) <= 0.000001) {\n"
           "        return 0.0;\n"
           "    }\n"
           "    return a / b;\n"
           "}\n\n"
           "fn ast_mod(a: f32, b: f32) -> f32 {\n"
           "    if (abs(b) <= 0.000001) {\n"
           "        return 0.0;\n"
           "    }\n"
           "    return a - floor(a / b) * b;\n"
           "}\n\n"
           "fn ast_eq(a: f32, b: f32) -> bool {\n"
           "    return abs(a - b) <= 0.000001;\n"
           "}\n\n"
           "fn ast_unorm(v: f32) -> f32 {\n"
           "    return clamp(v, 0.0, 1.0);\n"
           "}\n\n"
           "fn ast_byte(v: f32) -> f32 {\n"
           "    return clamp(v, 0.0, 255.0) / 255.0;\n"
           "}\n\n"
           "fn ast_channel_meta(channel: i32) -> vec4<f32> {\n"
           "    switch (channel) {\n"
           "    case 0: { return ast_inputs.channel0; }\n"
           "    case 1: { return ast_inputs.channel1; }\n"
           "    case 2: { return ast_inputs.channel2; }\n"
           "    case 3: { return ast_inputs.channel3; }\n"
           "    default: { return vec4<f32>(0.0); }\n"
           "    }\n"
           "}\n\n"
           "fn ast_channel_load(channel: i32, coord: vec2<i32>) -> vec4<f32> {\n"
           "    switch (channel) {\n"
           "    case 0: { return textureLoad(channel0_texture, coord, 0); }\n"
           "    case 1: { return textureLoad(channel1_texture, coord, 0); }\n"
           "    case 2: { return textureLoad(channel2_texture, coord, 0); }\n"
           "    case 3: { return textureLoad(channel3_texture, coord, 0); }\n"
           "    default: { return vec4<f32>(0.0); }\n"
           "    }\n"
           "}\n\n"
           "fn ast_packed_vec4_read(value: vec4<f32>, lane: i32) -> f32 {\n"
           "    switch (lane) {\n"
           "    case 0: { return value.x; }\n"
           "    case 1: { return value.y; }\n"
           "    case 2: { return value.z; }\n"
           "    case 3: { return value.w; }\n"
           "    default: { return 0.0; }\n"
           "    }\n"
           "}\n\n"
           "fn ast_key_state(scancode: i32) -> f32 {\n"
           "    if (scancode < 0 || scancode >= 512) {\n"
           "        return 0.0;\n"
           "    }\n"
           "    let bucket = scancode / 4;\n"
           "    let lane = scancode - bucket * 4;\n"
           "    return ast_packed_vec4_read(ast_inputs.keys[bucket], lane);\n"
           "}\n\n"
           "fn ast_mouse_button_state(button: i32) -> f32 {\n"
           "    if (button < 0 || button >= 8) {\n"
           "        return 0.0;\n"
           "    }\n"
           "    let bucket = button / 4;\n"
           "    let lane = button - bucket * 4;\n"
           "    return ast_packed_vec4_read(ast_inputs.mouse_buttons[bucket], lane);\n"
           "}\n\n"
           "fn ast_gamepad_button_state(button: i32) -> f32 {\n"
           "    if (button < 0 || button >= 32) {\n"
           "        return 0.0;\n"
           "    }\n"
           "    let bucket = button / 4;\n"
           "    let lane = button - bucket * 4;\n"
           "    return ast_packed_vec4_read(ast_inputs.gamepad_buttons[bucket], lane);\n"
           "}\n\n"
           "fn ast_gamepad_axis_state(axis: i32) -> f32 {\n"
           "    if (axis < 0 || axis >= 16) {\n"
           "        return 0.0;\n"
           "    }\n"
           "    let bucket = axis / 4;\n"
           "    let lane = axis - bucket * 4;\n"
           "    return ast_packed_vec4_read(ast_inputs.gamepad_axes[bucket], lane);\n"
           "}\n\n"
           "@compute @workgroup_size(8, 8, 1)\n"
           "fn main(@builtin(global_invocation_id) gid: vec3<u32>) {\n"
           "    if (gid.x >= u32(ast_inputs.width) || gid.y >= u32(ast_inputs.height)) {\n"
           "        return;\n"
           "    }\n"
           "    var r: array<f32, 64>;\n"
           "    r[0] = f32(gid.x);\n"
           "    r[1] = f32(gid.y);\n"
           "    r[2] = ast_inputs.time;\n"
           "    r[3] = ast_inputs.width;\n"
           "    r[4] = ast_inputs.height;\n"
           "    r[5] = ast_inputs.mouse_x;\n"
           "    r[6] = ast_inputs.mouse_y;\n"
           "    r[7] = ast_inputs.mouse_down;\n"
           "    r[8] = ast_inputs.mouse_click_x;\n"
           "    r[9] = ast_inputs.mouse_click_y;\n"
           "    r[10] = ast_inputs.frame;\n"
           "    r[11] = ast_inputs.time_delta;\n"
           "    r[12] = ast_inputs.wall_clock_seconds;\n"
           "    r[13] = ast_inputs.year;\n"
           "    r[14] = ast_inputs.month;\n"
           "    r[15] = ast_inputs.day;\n"
           "    var color = vec4<f32>(0.0, 0.0, 0.0, 1.0);\n"
           "    var pc: i32 = 0;\n"
           "    var steps: i32 = 0;\n"
           "    var call_stack: array<i32, 32>;\n"
           "    var call_depth: i32 = 0;\n"
           "    loop {\n"
        << "        if (pc < 0 || steps >= " << options.max_steps
        << ") {\n"
           "            break;\n"
           "        }\n"
           "        steps = steps + 1;\n"
           "        switch (pc) {\n";
}

void emit_footer(std::ostringstream& out) {
    out << "        default: {\n"
           "            pc = -1;\n"
           "        }\n"
           "        }\n"
           "    }\n"
           "    textureStore(output_texture, vec2<i32>(i32(gid.x), i32(gid.y)), color);\n"
           "}\n";
}

void emit_instruction(std::ostringstream& out, const IrInstruction& instruction, int pc,
                      int program_size, const WgslOptions& options) {
    const auto& o = instruction.operands;
    out << "        case " << pc << ": {\n";
    if (!instruction.file.empty()) {
        out << "            // " << instruction.file << ':' << instruction.line << '\n';
    }

    switch (instruction.op) {
    case Op::Mov:
        out << "            " << write_register(o[0], read_operand(o[1])) << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Add:
        out << "            "
            << write_register(o[0], read_operand(o[1]) + " + " + read_operand(o[2])) << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Sub:
        out << "            "
            << write_register(o[0], read_operand(o[1]) + " - " + read_operand(o[2])) << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Mul:
        out << "            "
            << write_register(o[0], read_operand(o[1]) + " * " + read_operand(o[2])) << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Div:
    case Op::Norm:
        out << "            "
            << write_register(o[0], "ast_safe_div(" + read_operand(o[1]) + ", " +
                                        read_operand(o[2]) + ")")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Sin:
        out << "            " << write_register(o[0], "sin(" + read_operand(o[1]) + ")") << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Cos:
        out << "            " << write_register(o[0], "cos(" + read_operand(o[1]) + ")") << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Sqrt:
        out << "            " << write_register(o[0], "sqrt(max(0.0, " + read_operand(o[1]) + "))")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Abs:
        out << "            " << write_register(o[0], "abs(" + read_operand(o[1]) + ")") << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Floor:
        out << "            " << write_register(o[0], "floor(" + read_operand(o[1]) + ")") << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Fract:
        out << "            "
            << write_register(o[0], read_operand(o[1]) + " - floor(" + read_operand(o[1]) + ")")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Min:
        out << "            "
            << write_register(o[0], "min(" + read_operand(o[1]) + ", " + read_operand(o[2]) + ")")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Max:
        out << "            "
            << write_register(o[0], "max(" + read_operand(o[1]) + ", " + read_operand(o[2]) + ")")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Mod:
        out << "            "
            << write_register(o[0],
                              "ast_mod(" + read_operand(o[1]) + ", " + read_operand(o[2]) + ")")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Lt:
        out << "            "
            << write_register(o[0], "select(0.0, 1.0, " + read_operand(o[1]) + " < " +
                                        read_operand(o[2]) + ")")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Gt:
        out << "            "
            << write_register(o[0], "select(0.0, 1.0, " + read_operand(o[1]) + " > " +
                                        read_operand(o[2]) + ")")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Eq:
        out << "            "
            << write_register(o[0], "select(0.0, 1.0, ast_eq(" + read_operand(o[1]) + ", " +
                                        read_operand(o[2]) + "))")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Jmp:
        out << "            pc = " << target_operand(o[0]) << ";\n";
        break;
    case Op::Jnz:
        out << "            if (" << read_operand(o[0]) << " != 0.0) {\n"
            << "                pc = " << target_operand(o[1]) << ";\n"
            << "            } else {\n"
            << "                pc = " << next_pc(pc) << ";\n"
            << "            }\n";
        break;
    case Op::Jz:
        out << "            if (" << read_operand(o[0]) << " == 0.0) {\n"
            << "                pc = " << target_operand(o[1]) << ";\n"
            << "            } else {\n"
            << "                pc = " << next_pc(pc) << ";\n"
            << "            }\n";
        break;
    case Op::Jeq:
        out << "            if (ast_eq(" << read_operand(o[0]) << ", " << read_operand(o[1])
            << ")) {\n"
            << "                pc = " << target_operand(o[2]) << ";\n"
            << "            } else {\n"
            << "                pc = " << next_pc(pc) << ";\n"
            << "            }\n";
        break;
    case Op::Jne:
        out << "            if (!ast_eq(" << read_operand(o[0]) << ", " << read_operand(o[1])
            << ")) {\n"
            << "                pc = " << target_operand(o[2]) << ";\n"
            << "            } else {\n"
            << "                pc = " << next_pc(pc) << ";\n"
            << "            }\n";
        break;
    case Op::Jlt:
        out << "            if (" << read_operand(o[0]) << " < " << read_operand(o[1]) << ") {\n"
            << "                pc = " << target_operand(o[2]) << ";\n"
            << "            } else {\n"
            << "                pc = " << next_pc(pc) << ";\n"
            << "            }\n";
        break;
    case Op::Jle:
        out << "            if (" << read_operand(o[0]) << " <= " << read_operand(o[1]) << ") {\n"
            << "                pc = " << target_operand(o[2]) << ";\n"
            << "            } else {\n"
            << "                pc = " << next_pc(pc) << ";\n"
            << "            }\n";
        break;
    case Op::Jgt:
        out << "            if (" << read_operand(o[0]) << " > " << read_operand(o[1]) << ") {\n"
            << "                pc = " << target_operand(o[2]) << ";\n"
            << "            } else {\n"
            << "                pc = " << next_pc(pc) << ";\n"
            << "            }\n";
        break;
    case Op::Jge:
        out << "            if (" << read_operand(o[0]) << " >= " << read_operand(o[1]) << ") {\n"
            << "                pc = " << target_operand(o[2]) << ";\n"
            << "            } else {\n"
            << "                pc = " << next_pc(pc) << ";\n"
            << "            }\n";
        break;
    case Op::Out:
        out << "            color = vec4<f32>(ast_unorm(" << read_operand(o[0]) << "), ast_unorm("
            << read_operand(o[1]) << "), ast_unorm(" << read_operand(o[2]) << "), ast_unorm("
            << read_operand(o[3]) << "));\n"
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Out8:
        out << "            color = vec4<f32>(ast_byte(" << read_operand(o[0]) << "), ast_byte("
            << read_operand(o[1]) << "), ast_byte(" << read_operand(o[2]) << "), ast_byte("
            << read_operand(o[3]) << "));\n"
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Tex:
        out << "            let tex_channel_" << pc << " = " << read_i32_operand(o[4]) << ";\n"
            << "            let tex_meta_" << pc << " = ast_channel_meta(tex_channel_" << pc
            << ");\n"
            << "            if (tex_meta_" << pc << ".x <= 0.0 || tex_meta_" << pc
            << ".y <= 0.0) {\n"
            << "                " << write_register(o[0], "0.0") << '\n'
            << "                " << write_register(o[1], "0.0") << '\n'
            << "                " << write_register(o[2], "0.0") << '\n'
            << "                " << write_register(o[3], "0.0") << '\n'
            << "            } else {\n"
            << "                let tex_w_" << pc << " = i32(tex_meta_" << pc << ".x);\n"
            << "                let tex_h_" << pc << " = i32(tex_meta_" << pc << ".y);\n"
            << "                let tex_x_" << pc << " = clamp(i32(floor(clamp("
            << read_operand(o[5]) << ", 0.0, 1.0) * tex_meta_" << pc << ".x)), 0, tex_w_" << pc
            << " - 1);\n"
            << "                let tex_y_" << pc << " = clamp(i32(floor(clamp("
            << read_operand(o[6]) << ", 0.0, 1.0) * tex_meta_" << pc << ".y)), 0, tex_h_" << pc
            << " - 1);\n"
            << "                let tex_sample_" << pc << " = ast_channel_load(tex_channel_" << pc
            << ", vec2<i32>(tex_x_" << pc << ", tex_y_" << pc << "));\n"
            << "                " << write_register(o[0], "tex_sample_" + std::to_string(pc) + ".r")
            << '\n'
            << "                " << write_register(o[1], "tex_sample_" + std::to_string(pc) + ".g")
            << '\n'
            << "                " << write_register(o[2], "tex_sample_" + std::to_string(pc) + ".b")
            << '\n'
            << "                " << write_register(o[3], "tex_sample_" + std::to_string(pc) + ".a")
            << '\n'
            << "            }\n"
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Texel:
        out << "            let texel_channel_" << pc << " = " << read_i32_operand(o[4]) << ";\n"
            << "            let texel_meta_" << pc << " = ast_channel_meta(texel_channel_" << pc
            << ");\n"
            << "            let texel_x_" << pc << " = i32(round(" << read_operand(o[5]) << "));\n"
            << "            let texel_y_" << pc << " = i32(round(" << read_operand(o[6]) << "));\n"
            << "            if (texel_meta_" << pc << ".x <= 0.0 || texel_meta_" << pc
            << ".y <= 0.0 || texel_x_" << pc << " < 0 || texel_y_" << pc << " < 0 || texel_x_" << pc
            << " >= i32(texel_meta_" << pc << ".x) || texel_y_" << pc << " >= i32(texel_meta_" << pc
            << ".y)) {\n"
            << "                " << write_register(o[0], "0.0") << '\n'
            << "                " << write_register(o[1], "0.0") << '\n'
            << "                " << write_register(o[2], "0.0") << '\n'
            << "                " << write_register(o[3], "0.0") << '\n'
            << "            } else {\n"
            << "                let texel_sample_" << pc << " = ast_channel_load(texel_channel_"
            << pc << ", vec2<i32>(texel_x_" << pc << ", texel_y_" << pc << "));\n"
            << "                "
            << write_register(o[0], "texel_sample_" + std::to_string(pc) + ".r") << '\n'
            << "                "
            << write_register(o[1], "texel_sample_" + std::to_string(pc) + ".g") << '\n'
            << "                "
            << write_register(o[2], "texel_sample_" + std::to_string(pc) + ".b") << '\n'
            << "                "
            << write_register(o[3], "texel_sample_" + std::to_string(pc) + ".a") << '\n'
            << "            }\n"
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Chdim:
        out << "            let chdim_meta_" << pc << " = ast_channel_meta("
            << read_i32_operand(o[2]) << ");\n"
            << "            " << write_register(o[0], "chdim_meta_" + std::to_string(pc) + ".x")
            << '\n'
            << "            " << write_register(o[1], "chdim_meta_" + std::to_string(pc) + ".y")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Chtime:
        out << "            let chtime_meta_" << pc << " = ast_channel_meta("
            << read_i32_operand(o[1]) << ");\n"
            << "            " << write_register(o[0], "chtime_meta_" + std::to_string(pc) + ".z")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Chsrate:
        out << "            let chsrate_meta_" << pc << " = ast_channel_meta("
            << read_i32_operand(o[1]) << ");\n"
            << "            " << write_register(o[0], "chsrate_meta_" + std::to_string(pc) + ".w")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Key:
        out << "            "
            << write_register(o[0], "ast_key_state(i32(" + read_operand(o[1]) + "))") << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Mbtn:
        out << "            "
            << write_register(o[0], "ast_mouse_button_state(i32(" + read_operand(o[1]) + "))")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Mwheel:
        out << "            " << write_register(o[0], "ast_inputs.mouse_wheel.x") << '\n'
            << "            " << write_register(o[1], "ast_inputs.mouse_wheel.y") << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Gbtn:
        out << "            "
            << write_register(o[0], "ast_gamepad_button_state(i32(" + read_operand(o[1]) + "))")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Gaxis:
        out << "            "
            << write_register(o[0], "ast_gamepad_axis_state(i32(" + read_operand(o[1]) + "))")
            << '\n'
            << "            pc = " << next_pc(pc) << ";\n";
        break;
    case Op::Call:
        out << "            if (call_depth >= " << options.max_call_depth << ") {\n"
            << "                pc = -1;\n"
            << "            } else {\n"
            << "                call_stack[call_depth] = " << next_pc(pc) << ";\n"
            << "                call_depth = call_depth + 1;\n"
            << "                pc = " << target_operand(o[0]) << ";\n"
            << "            }\n";
        break;
    case Op::Ret:
        out << "            if (call_depth > 0) {\n"
            << "                call_depth = call_depth - 1;\n"
            << "                pc = call_stack[call_depth];\n"
            << "            } else {\n"
            << "                pc = " << program_size << ";\n"
            << "            }\n";
        break;
    case Op::Halt:
        out << "            pc = " << program_size << ";\n";
        break;
    }
    out << "        }\n";
}

} // namespace

WgslCompileResult compile_wgsl(const IrProgram& program, const WgslOptions& options) {
    WgslCompileResult result;
    const WgslOptions effective_options{std::max(1, options.max_steps),
                                        std::clamp(options.max_call_depth, 0, 32)};
    for (const IrInstruction& instruction : program.code) {
        if (!supported_op(instruction.op)) {
            add_diag(result.diagnostics, instruction,
                     "opcode '" + op_name(instruction.op) +
                         "' is not supported by WGSL emitter yet");
        }
    }
    if (!result.ok()) {
        return result;
    }

    std::ostringstream out;
    emit_header(out, effective_options);
    for (int pc = 0; pc < static_cast<int>(program.code.size()); ++pc) {
        emit_instruction(out, program.code[static_cast<std::size_t>(pc)], pc,
                         static_cast<int>(program.code.size()), effective_options);
    }
    emit_footer(out);
    result.source = out.str();
    return result;
}

WgslCompileResult compile_wgsl(const Program& program, const WgslOptions& options) {
    const LowerIrResult lowered = lower_to_ir(program);
    WgslCompileResult result;
    if (!lowered.ok()) {
        result.diagnostics = lowered.diagnostics;
        return result;
    }
    return compile_wgsl(lowered.program, options);
}

} // namespace ast
