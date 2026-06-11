#pragma once

#include "ast/assembler.hpp"
#include "ast/ir.hpp"

#include <string>
#include <vector>

namespace ast {

struct WgslOptions {
    int max_steps = 4096;
    int max_call_depth = 32;
};

struct WgslCompileResult {
    std::string source;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const {
        return diagnostics.empty();
    }
};

WgslCompileResult compile_wgsl(const IrProgram& program, const WgslOptions& options = {});
WgslCompileResult compile_wgsl(const Program& program, const WgslOptions& options = {});

} // namespace ast
