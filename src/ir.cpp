#include "ast/ir.hpp"

#include <cmath>
#include <cstddef>
#include <string>

namespace ast {
namespace {

void add_diag(std::vector<Diagnostic>& diagnostics, const Instruction& instruction,
              std::string message) {
    diagnostics.push_back(Diagnostic{instruction.file, instruction.line, std::move(message)});
}

bool is_jump_target_operand(Op op, int index) {
    switch (op) {
    case Op::Jmp:
    case Op::Call:
        return index == 0;
    case Op::Jnz:
    case Op::Jz:
        return index == 1;
    case Op::Jeq:
    case Op::Jne:
    case Op::Jlt:
    case Op::Jle:
    case Op::Jgt:
    case Op::Jge:
        return index == 2;
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
    case Op::Ret:
    case Op::Halt:
        return false;
    }
    return false;
}

int jump_target(const Operand& operand) {
    return static_cast<int>(operand.value);
}

bool valid_jump_target(const Program& program, int target) {
    return target >= 0 && target < static_cast<int>(program.code.size());
}

void add_feature(IrInstruction& instruction, IrFeature feature) {
    instruction.features.push_back(feature);
}

void classify_instruction(IrProgram& program, IrInstruction& instruction) {
    switch (instruction.op) {
    case Op::Tex:
    case Op::Texel:
        program.has_textures = true;
        add_feature(instruction, IrFeature::Texture);
        break;
    case Op::Chdim:
    case Op::Chtime:
    case Op::Chsrate:
        program.has_channel_metadata = true;
        add_feature(instruction, IrFeature::ChannelMetadata);
        break;
    case Op::Key:
    case Op::Mbtn:
    case Op::Mwheel:
    case Op::Gbtn:
    case Op::Gaxis:
        program.has_live_input = true;
        add_feature(instruction, IrFeature::LiveInput);
        break;
    case Op::Jmp:
    case Op::Jnz:
    case Op::Jz:
    case Op::Jeq:
    case Op::Jne:
    case Op::Jlt:
    case Op::Jle:
    case Op::Jgt:
    case Op::Jge:
        program.has_control_flow = true;
        add_feature(instruction, IrFeature::ControlFlow);
        break;
    case Op::Call:
    case Op::Ret:
        program.has_control_flow = true;
        program.has_calls = true;
        add_feature(instruction, IrFeature::ControlFlow);
        add_feature(instruction, IrFeature::Calls);
        break;
    case Op::Out:
    case Op::Out8:
        program.has_output = true;
        add_feature(instruction, IrFeature::Output);
        break;
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
    case Op::Halt:
        break;
    }
}

void add_successor(IrInstruction& instruction, int target) {
    if (instruction.successor_count >= static_cast<int>(instruction.successors.size())) {
        return;
    }
    instruction.successors[static_cast<std::size_t>(instruction.successor_count)] = target;
    ++instruction.successor_count;
}

void compute_successors(const Program& source, int pc, IrInstruction& instruction,
                        std::vector<Diagnostic>& diagnostics) {
    auto add_fallthrough = [&]() {
        const int next = pc + 1;
        if (next < static_cast<int>(source.code.size())) {
            add_successor(instruction, next);
        }
    };
    auto add_target = [&](int operand_index) {
        const Operand& operand = instruction.operands[static_cast<std::size_t>(operand_index)];
        const int target = jump_target(operand);
        if (!valid_jump_target(source, target)) {
            add_diag(diagnostics, source.code[static_cast<std::size_t>(pc)],
                     "jump target is outside the program");
            return;
        }
        add_successor(instruction, target);
    };

    switch (instruction.op) {
    case Op::Jmp:
    case Op::Call:
        add_target(0);
        break;
    case Op::Jnz:
    case Op::Jz:
        add_target(1);
        add_fallthrough();
        break;
    case Op::Jeq:
    case Op::Jne:
    case Op::Jlt:
    case Op::Jle:
    case Op::Jgt:
    case Op::Jge:
        add_target(2);
        add_fallthrough();
        break;
    case Op::Ret:
    case Op::Halt:
        break;
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
        add_fallthrough();
        break;
    }
}

} // namespace

LowerIrResult lower_to_ir(const Program& program) {
    LowerIrResult result;
    result.program.code.reserve(program.code.size());

    for (int pc = 0; pc < static_cast<int>(program.code.size()); ++pc) {
        const Instruction& source = program.code[static_cast<std::size_t>(pc)];
        IrInstruction lowered;
        lowered.op = source.op;
        lowered.operands = source.operands;
        lowered.operand_count = source.operand_count;
        lowered.line = source.line;
        lowered.file = source.file;

        for (int i = 0; i < source.operand_count; ++i) {
            const Operand& operand = source.operands[static_cast<std::size_t>(i)];
            if (operand.kind == OperandKind::Register &&
                (operand.reg < 0 || operand.reg >= register_count)) {
                add_diag(result.diagnostics, source, "register operand is out of range");
            }
            if (is_jump_target_operand(source.op, i)) {
                if (operand.kind != OperandKind::Immediate ||
                    std::fabs(operand.value - std::round(operand.value)) > 0.000001F) {
                    add_diag(result.diagnostics, source,
                             "jump target must be an instruction index");
                }
            }
        }

        classify_instruction(result.program, lowered);
        compute_successors(program, pc, lowered, result.diagnostics);
        result.program.code.push_back(std::move(lowered));
    }

    return result;
}

} // namespace ast
