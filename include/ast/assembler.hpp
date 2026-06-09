#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ast {

constexpr int register_count = 32;

enum class Op {
    Mov,
    Add,
    Sub,
    Mul,
    Div,
    Sin,
    Cos,
    Sqrt,
    Abs,
    Floor,
    Fract,
    Min,
    Max,
    Mod,
    Norm,
    Lt,
    Gt,
    Eq,
    Jmp,
    Jnz,
    Out,
    Out8,
    Ret,
};

enum class OperandKind {
    Register,
    Immediate,
};

struct Operand {
    OperandKind kind = OperandKind::Immediate;
    int reg = 0;
    float value = 0.0F;
};

struct Instruction {
    Op op = Op::Ret;
    std::vector<Operand> operands;
    int line = 0;
    std::string file;
};

struct Program {
    std::vector<Instruction> code;
};

struct Diagnostic {
    std::string file;
    int line = 0;
    std::string message;
};

struct AssembleResult {
    Program program;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const {
        return diagnostics.empty();
    }
};

AssembleResult assemble_source(std::string source, std::string file_name = "<memory>");
AssembleResult assemble_file(const std::filesystem::path& path);

} // namespace ast
