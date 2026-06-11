#pragma once

#include "ast/assembler.hpp"

#include <array>
#include <string>
#include <vector>

namespace ast {

enum class IrFeature {
    Texture,
    ChannelMetadata,
    LiveInput,
    ControlFlow,
    Calls,
    Output,
};

struct IrInstruction {
    Op op = Op::Ret;
    std::array<Operand, 7> operands;
    int operand_count = 0;
    int line = 0;
    std::string file;
    std::array<int, 2> successors{-1, -1};
    int successor_count = 0;
    std::vector<IrFeature> features;
};

struct IrProgram {
    std::vector<IrInstruction> code;
    bool has_control_flow = false;
    bool has_calls = false;
    bool has_textures = false;
    bool has_channel_metadata = false;
    bool has_live_input = false;
    bool has_output = false;
};

struct LowerIrResult {
    IrProgram program;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const {
        return diagnostics.empty();
    }
};

LowerIrResult lower_to_ir(const Program& program);

} // namespace ast
