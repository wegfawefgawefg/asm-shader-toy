#include "ast/assembler.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>

namespace ast {
namespace {

struct RawInstruction {
    std::string op;
    std::vector<std::string> args;
    int line = 0;
    std::string file;
};

struct ParseState {
    std::vector<RawInstruction> raw;
    std::map<std::string, int> labels;
    std::map<std::string, float> constants;
    std::vector<Diagnostic> diagnostics;
    std::set<std::filesystem::path> include_stack;
};

std::string trim(std::string_view text) {
    std::size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])) != 0) {
        ++first;
    }

    std::size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])) != 0) {
        --last;
    }

    return std::string{text.substr(first, last - first)};
}

std::string lower(std::string text) {
    std::ranges::transform(text, text.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::string strip_comment(std::string line) {
    bool in_quote = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            in_quote = !in_quote;
        }
        if (in_quote) {
            continue;
        }
        if (ch == ';' || ch == '#') {
            return line.substr(0, i);
        }
        if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            return line.substr(0, i);
        }
    }
    return line;
}

std::vector<std::string> split_tokens(std::string_view line) {
    std::vector<std::string> out;
    std::string current;
    bool in_quote = false;

    for (char ch : line) {
        if (ch == '"') {
            in_quote = !in_quote;
            current.push_back(ch);
            continue;
        }
        if (!in_quote && (ch == ',' || std::isspace(static_cast<unsigned char>(ch)) != 0)) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }

    if (!current.empty()) {
        out.push_back(current);
    }

    return out;
}

std::optional<float> parse_float(std::string_view token) {
    float value = 0.0F;
    const char* begin = token.data();
    const char* end = begin + token.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

std::optional<int> parse_register(std::string_view token) {
    if (token.size() < 2 || token[0] != 'r') {
        return std::nullopt;
    }

    int value = 0;
    const char* begin = token.data() + 1;
    const char* end = token.data() + token.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end || value < 0 || value >= register_count) {
        return std::nullopt;
    }
    return value;
}

void add_diag(ParseState& state, const std::string& file, int line, std::string message) {
    state.diagnostics.push_back(Diagnostic{file, line, std::move(message)});
}

void parse_source(ParseState& state, const std::string& source, const std::string& file,
                  const std::filesystem::path& base_dir);

void parse_include(ParseState& state, const std::string& token, const std::string& file, int line,
                   const std::filesystem::path& base_dir) {
    if (token.size() < 2 || token.front() != '"' || token.back() != '"') {
        add_diag(state, file, line, ".include expects a quoted path");
        return;
    }

    const std::filesystem::path include_path = base_dir / token.substr(1, token.size() - 2);
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(include_path).lexically_normal();
    if (state.include_stack.contains(canonical)) {
        add_diag(state, file, line, "recursive include: " + canonical.string());
        return;
    }

    std::ifstream input(canonical);
    if (!input) {
        add_diag(state, file, line, "could not open include: " + canonical.string());
        return;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    state.include_stack.insert(canonical);
    parse_source(state, buffer.str(), canonical.string(), canonical.parent_path());
    state.include_stack.erase(canonical);
}

void parse_source(ParseState& state, const std::string& source, const std::string& file,
                  const std::filesystem::path& base_dir) {
    std::istringstream stream(source);
    std::string line;
    int line_no = 0;

    while (std::getline(stream, line)) {
        ++line_no;
        line = trim(strip_comment(line));
        if (line.empty()) {
            continue;
        }

        if (line.back() == ':') {
            const std::string label =
                lower(trim(std::string_view{line}.substr(0, line.size() - 1)));
            if (label.empty()) {
                add_diag(state, file, line_no, "empty label");
                continue;
            }
            state.labels[label] = static_cast<int>(state.raw.size());
            continue;
        }

        std::vector<std::string> tokens = split_tokens(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string directive = lower(tokens[0]);
        if (directive == ".include") {
            if (tokens.size() != 2) {
                add_diag(state, file, line_no, ".include expects exactly one path");
                continue;
            }
            parse_include(state, tokens[1], file, line_no, base_dir);
            continue;
        }

        if (directive == ".const") {
            if (tokens.size() != 3) {
                add_diag(state, file, line_no, ".const expects name and value");
                continue;
            }
            const auto value = parse_float(tokens[2]);
            if (!value.has_value()) {
                add_diag(state, file, line_no, "invalid .const value: " + tokens[2]);
                continue;
            }
            state.constants[lower(tokens[1])] = *value;
            continue;
        }

        RawInstruction raw;
        raw.op = directive;
        raw.line = line_no;
        raw.file = file;
        for (std::size_t i = 1; i < tokens.size(); ++i) {
            raw.args.push_back(lower(tokens[i]));
        }
        state.raw.push_back(std::move(raw));
    }
}

std::optional<Op> parse_op(std::string_view op) {
    static const std::map<std::string_view, Op> ops{
        {"mov", Op::Mov}, {"add", Op::Add},     {"sub", Op::Sub},     {"mul", Op::Mul},
        {"div", Op::Div}, {"sin", Op::Sin},     {"cos", Op::Cos},     {"sqrt", Op::Sqrt},
        {"abs", Op::Abs}, {"floor", Op::Floor}, {"fract", Op::Fract}, {"min", Op::Min},
        {"max", Op::Max}, {"mod", Op::Mod},     {"norm", Op::Norm},   {"lt", Op::Lt},
        {"gt", Op::Gt},   {"eq", Op::Eq},       {"jmp", Op::Jmp},     {"jnz", Op::Jnz},
        {"out", Op::Out}, {"out8", Op::Out8},   {"ret", Op::Ret},
    };

    const auto it = ops.find(op);
    if (it == ops.end()) {
        return std::nullopt;
    }
    return it->second;
}

int expected_operands(Op op) {
    switch (op) {
    case Op::Ret:
        return 0;
    case Op::Sin:
    case Op::Cos:
    case Op::Sqrt:
    case Op::Abs:
    case Op::Floor:
    case Op::Fract:
    case Op::Jmp:
        return 1;
    case Op::Mov:
    case Op::Jnz:
        return 2;
    case Op::Add:
    case Op::Sub:
    case Op::Mul:
    case Op::Div:
    case Op::Min:
    case Op::Max:
    case Op::Mod:
    case Op::Norm:
    case Op::Lt:
    case Op::Gt:
    case Op::Eq:
        return 3;
    case Op::Out:
    case Op::Out8:
        return 4;
    }
    return 0;
}

bool operand_must_be_register(Op op, int index) {
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
        return index == 0;
    case Op::Jmp:
    case Op::Jnz:
    case Op::Out:
    case Op::Out8:
    case Op::Ret:
        return false;
    }
    return false;
}

std::optional<Operand> parse_operand(ParseState& state, const RawInstruction& raw, Op op,
                                     int index) {
    const std::string& token = raw.args[static_cast<std::size_t>(index)];
    if (const auto reg = parse_register(token); reg.has_value()) {
        return Operand{OperandKind::Register, *reg, 0.0F};
    }

    if (operand_must_be_register(op, index)) {
        add_diag(state, raw.file, raw.line, "destination must be a register: " + token);
        return std::nullopt;
    }

    if ((op == Op::Jmp && index == 0) || (op == Op::Jnz && index == 1)) {
        const auto label = state.labels.find(token);
        if (label == state.labels.end()) {
            add_diag(state, raw.file, raw.line, "unknown label: " + token);
            return std::nullopt;
        }
        return Operand{OperandKind::Immediate, 0, static_cast<float>(label->second)};
    }

    if (const auto constant = state.constants.find(token); constant != state.constants.end()) {
        return Operand{OperandKind::Immediate, 0, constant->second};
    }

    if (const auto value = parse_float(token); value.has_value()) {
        return Operand{OperandKind::Immediate, 0, *value};
    }

    add_diag(state, raw.file, raw.line, "invalid operand: " + token);
    return std::nullopt;
}

Program lower_program(ParseState& state) {
    Program program;

    for (const RawInstruction& raw : state.raw) {
        const auto parsed_op = parse_op(raw.op);
        if (!parsed_op.has_value()) {
            add_diag(state, raw.file, raw.line, "unknown instruction: " + raw.op);
            continue;
        }

        const Op op = *parsed_op;
        const int expected = expected_operands(op);
        if (static_cast<int>(raw.args.size()) != expected) {
            add_diag(state, raw.file, raw.line,
                     raw.op + " expects " + std::to_string(expected) + " operands");
            continue;
        }

        Instruction instruction;
        instruction.op = op;
        instruction.line = raw.line;
        instruction.file = raw.file;

        bool failed = false;
        for (int i = 0; i < expected; ++i) {
            const auto operand = parse_operand(state, raw, op, i);
            if (!operand.has_value()) {
                failed = true;
                break;
            }
            instruction.operands.push_back(*operand);
        }
        if (!failed) {
            program.code.push_back(std::move(instruction));
        }
    }

    return program;
}

} // namespace

AssembleResult assemble_source(std::string source, std::string file_name) {
    ParseState state;
    parse_source(state, source, file_name, std::filesystem::current_path());
    AssembleResult result;
    result.program = lower_program(state);
    result.diagnostics = std::move(state.diagnostics);
    return result;
}

AssembleResult assemble_file(const std::filesystem::path& path) {
    ParseState state;
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(path).lexically_normal();
    std::ifstream input(canonical);
    if (!input) {
        return AssembleResult{Program{},
                              {Diagnostic{canonical.string(), 0, "could not open file"}}};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    state.include_stack.insert(canonical);
    parse_source(state, buffer.str(), canonical.string(), canonical.parent_path());

    AssembleResult result;
    result.program = lower_program(state);
    result.diagnostics = std::move(state.diagnostics);
    return result;
}

} // namespace ast
