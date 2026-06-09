#include "ast/assembler.hpp"

#include "ast/vm_executor.hpp"

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

#ifndef AST_STDLIB_DIR
#define AST_STDLIB_DIR "stdlib"
#endif

struct RawInstruction {
    std::string op;
    std::vector<std::string> args;
    int line = 0;
    std::string file;
    std::string label_scope;
};

struct Alias {
    int reg = 0;
    bool writable = false;
    bool protected_name = false;
};

struct ConstBlock {
    std::vector<RawInstruction> raw;
    std::map<std::string, int> labels;
    std::string current_global_label;
    int start_line = 0;
};

struct ConstProgram {
    Program program;
    std::vector<std::string> slot_names;
    std::vector<float> slot_values;
    std::vector<bool> assigned;
    std::map<std::string, int> slots;
};

struct ParseState {
    std::vector<RawInstruction> raw;
    std::map<std::string, int> labels;
    std::map<std::string, float> constants;
    std::map<std::string, Alias> aliases;
    std::vector<Diagnostic> diagnostics;
    std::set<std::filesystem::path> include_stack;
    std::set<std::filesystem::path> included_files;
    std::vector<std::filesystem::path> dependencies;
    std::string current_global_label;
};

void add_diag(ParseState& state, const std::string& file, int line, std::string message);
std::optional<Op> parse_op(std::string_view op);
int expected_operands(Op op);
bool operand_must_be_register(Op op, int index);
bool operand_must_be_label(Op op, int index);
std::string resolve_label_token(const RawInstruction& raw, const std::string& token);
void execute_const_block(ParseState& state, const ConstBlock& block, const std::string& file);

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

bool is_local_label(std::string_view label) {
    return !label.empty() && label.front() == '.';
}

std::string scoped_label_name(ParseState& state, const std::string& label, const std::string& file,
                              int line) {
    if (!is_local_label(label)) {
        state.current_global_label = label;
        return label;
    }

    if (state.current_global_label.empty()) {
        add_diag(state, file, line, "local label requires a preceding global label: " + label);
        return {};
    }

    return state.current_global_label + label;
}

std::string scoped_const_label_name(ParseState& state, ConstBlock& block, const std::string& label,
                                    const std::string& file, int line) {
    if (!is_local_label(label)) {
        block.current_global_label = label;
        return label;
    }

    if (block.current_global_label.empty()) {
        add_diag(state, file, line, "local label requires a preceding global label: " + label);
        return {};
    }

    return block.current_global_label + label;
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

void seed_builtin_aliases(ParseState& state) {
    state.aliases = {
        {"px", Alias{0, false, true}},
        {"pixel_x", Alias{0, false, true}},
        {"py", Alias{1, false, true}},
        {"pixel_y", Alias{1, false, true}},
        {"time", Alias{2, false, true}},
        {"width", Alias{3, false, true}},
        {"height", Alias{4, false, true}},
        {"mouse_x", Alias{5, false, true}},
        {"mouse_y", Alias{6, false, true}},
        {"mouse_down", Alias{7, false, true}},
        {"mouse_click_x", Alias{8, false, true}},
        {"mouse_click_y", Alias{9, false, true}},
        {"frame", Alias{10, false, true}},
        {"time_delta", Alias{11, false, true}},
        {"wall_seconds", Alias{12, false, true}},
        {"date_year", Alias{13, false, true}},
        {"date_month", Alias{14, false, true}},
        {"date_day", Alias{15, false, true}},
    };
}

void add_diag(ParseState& state, const std::string& file, int line, std::string message) {
    state.diagnostics.push_back(Diagnostic{file, line, std::move(message)});
}

void parse_source(ParseState& state, const std::string& source, const std::string& file,
                  const std::filesystem::path& base_dir, bool protect_new_aliases);

std::optional<std::filesystem::path> resolve_include(ParseState& state, const std::string& token,
                                                     const std::string& file, int line,
                                                     const std::filesystem::path& base_dir) {
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        return base_dir / token.substr(1, token.size() - 2);
    }

    if (token.size() >= 2 && token.front() == '<' && token.back() == '>') {
        const std::filesystem::path relative = token.substr(1, token.size() - 2);
        if (relative.is_absolute()) {
            add_diag(state, file, line, "standard include must be relative: " + relative.string());
            return std::nullopt;
        }
        return std::filesystem::path{AST_STDLIB_DIR} / relative;
    }

    add_diag(state, file, line, ".include expects a quoted path or <standard/path.inc>");
    return std::nullopt;
}

void parse_include(ParseState& state, const std::string& token, const std::string& file, int line,
                   const std::filesystem::path& base_dir) {
    const std::optional<std::filesystem::path> include_path =
        resolve_include(state, token, file, line, base_dir);
    if (!include_path.has_value()) {
        return;
    }

    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(*include_path).lexically_normal();
    if (state.include_stack.contains(canonical)) {
        add_diag(state, file, line, "recursive include: " + canonical.string());
        return;
    }
    if (state.included_files.contains(canonical)) {
        return;
    }

    state.dependencies.push_back(canonical);
    std::ifstream input(canonical);
    if (!input) {
        add_diag(state, file, line, "could not open include: " + canonical.string());
        return;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    state.include_stack.insert(canonical);
    state.included_files.insert(canonical);
    const std::filesystem::path stdlib_root =
        std::filesystem::weakly_canonical(std::filesystem::path{AST_STDLIB_DIR}).lexically_normal();
    const bool protect_new_aliases =
        std::filesystem::relative(canonical, stdlib_root).native().starts_with("..") == false;
    parse_source(state, buffer.str(), canonical.string(), canonical.parent_path(),
                 protect_new_aliases);
    state.include_stack.erase(canonical);
}

void parse_const_block(ParseState& state, std::istringstream& stream, int& line_no,
                       const std::string& file) {
    ConstBlock block;
    block.start_line = line_no;
    std::string line;

    while (std::getline(stream, line)) {
        ++line_no;
        line = trim(strip_comment(line));
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = split_tokens(line);
        if (!tokens.empty() && lower(tokens[0]) == ".end") {
            if (tokens.size() != 1) {
                add_diag(state, file, line_no, ".end expects no operands");
                return;
            }
            execute_const_block(state, block, file);
            return;
        }

        if (line.back() == ':') {
            const std::string parsed_label =
                lower(trim(std::string_view{line}.substr(0, line.size() - 1)));
            if (parsed_label.empty()) {
                add_diag(state, file, line_no, "empty label");
                continue;
            }
            const std::string label =
                scoped_const_label_name(state, block, parsed_label, file, line_no);
            if (label.empty()) {
                continue;
            }
            block.labels[label] = static_cast<int>(block.raw.size());
            continue;
        }

        if (!tokens.empty() && lower(tokens[0]) == ".consts") {
            add_diag(state, file, line_no, "nested .consts blocks are not supported");
            continue;
        }

        RawInstruction raw;
        raw.op = lower(tokens[0]);
        raw.line = line_no;
        raw.file = file;
        raw.label_scope = block.current_global_label;
        for (std::size_t i = 1; i < tokens.size(); ++i) {
            raw.args.push_back(lower(tokens[i]));
        }
        block.raw.push_back(std::move(raw));
    }

    add_diag(state, file, block.start_line, ".consts missing .end");
}

void parse_source(ParseState& state, const std::string& source, const std::string& file,
                  const std::filesystem::path& base_dir, bool protect_new_aliases) {
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
            const std::string parsed_label =
                lower(trim(std::string_view{line}.substr(0, line.size() - 1)));
            if (parsed_label.empty()) {
                add_diag(state, file, line_no, "empty label");
                continue;
            }
            const std::string label = scoped_label_name(state, parsed_label, file, line_no);
            if (label.empty()) {
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
        if (directive == ".consts") {
            if (tokens.size() != 1) {
                add_diag(state, file, line_no, ".consts expects no operands");
                continue;
            }
            parse_const_block(state, stream, line_no, file);
            continue;
        }

        if (directive == ".end") {
            add_diag(state, file, line_no, ".end without .consts");
            continue;
        }

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

        if (directive == ".alias") {
            if (tokens.size() != 3) {
                add_diag(state, file, line_no, ".alias expects name and register");
                continue;
            }
            const std::string alias_name = lower(tokens[1]);
            if (const auto existing = state.aliases.find(alias_name);
                existing != state.aliases.end()) {
                if (existing->second.protected_name) {
                    add_diag(state, file, line_no, "alias name is reserved: " + alias_name);
                    continue;
                }
                if (protect_new_aliases) {
                    add_diag(state, file, line_no,
                             "alias conflicts with standard alias: " + alias_name);
                    continue;
                }
            }
            const auto reg = parse_register(lower(tokens[2]));
            if (!reg.has_value()) {
                add_diag(state, file, line_no, ".alias expects a register like r16");
                continue;
            }
            if (*reg < 16) {
                add_diag(state, file, line_no, ".alias may only name scratch registers r16..r63");
                continue;
            }
            state.aliases[alias_name] = Alias{*reg, true, protect_new_aliases};
            continue;
        }

        RawInstruction raw;
        raw.op = directive;
        raw.line = line_no;
        raw.file = file;
        raw.label_scope = state.current_global_label;
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
        {"jz", Op::Jz},   {"jeq", Op::Jeq},     {"jne", Op::Jne},     {"jlt", Op::Jlt},
        {"jle", Op::Jle}, {"jgt", Op::Jgt},     {"jge", Op::Jge},     {"call", Op::Call},
        {"out", Op::Out}, {"out8", Op::Out8},   {"tex", Op::Tex},     {"texel", Op::Texel},
        {"ret", Op::Ret}, {"halt", Op::Halt},
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
    case Op::Halt:
        return 0;
    case Op::Sin:
    case Op::Cos:
    case Op::Sqrt:
    case Op::Abs:
    case Op::Floor:
    case Op::Fract:
    case Op::Mov:
        return 2;
    case Op::Jmp:
    case Op::Call:
        return 1;
    case Op::Jnz:
    case Op::Jz:
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
    case Op::Jeq:
    case Op::Jne:
    case Op::Jlt:
    case Op::Jle:
    case Op::Jgt:
    case Op::Jge:
        return 3;
    case Op::Out:
    case Op::Out8:
        return 4;
    case Op::Tex:
    case Op::Texel:
        return 7;
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
    case Op::Tex:
    case Op::Texel:
        return index >= 0 && index <= 3;
    case Op::Jmp:
    case Op::Jnz:
    case Op::Jz:
    case Op::Jeq:
    case Op::Jne:
    case Op::Jlt:
    case Op::Jle:
    case Op::Jgt:
    case Op::Jge:
    case Op::Call:
    case Op::Out:
    case Op::Out8:
    case Op::Ret:
    case Op::Halt:
        return false;
    }
    return false;
}

bool operand_must_be_label(Op op, int index) {
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
    case Op::Ret:
    case Op::Halt:
        return false;
    }
    return false;
}

std::string resolve_label_token(const RawInstruction& raw, const std::string& token) {
    if (is_local_label(token) && !raw.label_scope.empty()) {
        return raw.label_scope + token;
    }
    return token;
}

std::optional<Operand> parse_operand(ParseState& state, const RawInstruction& raw, Op op,
                                     int index) {
    const std::string& token = raw.args[static_cast<std::size_t>(index)];
    if (const auto reg = parse_register(token); reg.has_value()) {
        return Operand{OperandKind::Register, *reg, 0.0F};
    }

    if (const auto alias = state.aliases.find(token); alias != state.aliases.end()) {
        if (operand_must_be_register(op, index) && !alias->second.writable) {
            add_diag(state, raw.file, raw.line, "input alias is read-only: " + token);
            return std::nullopt;
        }
        return Operand{OperandKind::Register, alias->second.reg, 0.0F};
    }

    if (operand_must_be_register(op, index)) {
        add_diag(state, raw.file, raw.line, "destination must be a register: " + token);
        return std::nullopt;
    }

    if (operand_must_be_label(op, index)) {
        const std::string label_token = resolve_label_token(raw, token);
        const auto label = state.labels.find(label_token);
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

bool op_allowed_in_consts(Op op) {
    switch (op) {
    case Op::Tex:
    case Op::Texel:
    case Op::Out:
    case Op::Out8:
        return false;
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
    case Op::Call:
    case Op::Ret:
    case Op::Halt:
        return true;
    }
    return false;
}

int const_slot(ConstProgram& program, ParseState& state, const std::string& name) {
    if (const auto found = program.slots.find(name); found != program.slots.end()) {
        return found->second;
    }

    const int index = static_cast<int>(program.slot_names.size());
    program.slots[name] = index;
    program.slot_names.push_back(name);
    if (const auto constant = state.constants.find(name); constant != state.constants.end()) {
        program.slot_values.push_back(constant->second);
    } else {
        program.slot_values.push_back(0.0F);
    }
    program.assigned.push_back(false);
    return index;
}

bool is_const_slot_destination(Op op, int index) {
    return operand_must_be_register(op, index);
}

void predeclare_const_slots(ConstProgram& program, ParseState& state, const ConstBlock& block) {
    for (const RawInstruction& raw : block.raw) {
        const auto parsed_op = parse_op(raw.op);
        if (!parsed_op.has_value() || !op_allowed_in_consts(*parsed_op)) {
            continue;
        }
        const int expected = expected_operands(*parsed_op);
        if (static_cast<int>(raw.args.size()) != expected) {
            continue;
        }
        for (int i = 0; i < expected; ++i) {
            if (!is_const_slot_destination(*parsed_op, i)) {
                continue;
            }
            const std::string& token = raw.args[static_cast<std::size_t>(i)];
            if (parse_float(token).has_value() || parse_register(token).has_value() ||
                state.aliases.contains(token)) {
                continue;
            }
            const_slot(program, state, token);
        }
    }
}

std::optional<Operand> parse_const_operand(ParseState& state, ConstProgram& const_program,
                                           const ConstBlock& block, const RawInstruction& raw,
                                           Op op, int index) {
    const std::string& token = raw.args[static_cast<std::size_t>(index)];

    if (operand_must_be_label(op, index)) {
        const std::string label_token = resolve_label_token(raw, token);
        const auto label = block.labels.find(label_token);
        if (label == block.labels.end()) {
            add_diag(state, raw.file, raw.line, "unknown label: " + token);
            return std::nullopt;
        }
        return Operand{OperandKind::Immediate, 0, static_cast<float>(label->second)};
    }

    if (is_const_slot_destination(op, index)) {
        if (parse_float(token).has_value()) {
            add_diag(state, raw.file, raw.line, "const destination must be a name: " + token);
            return std::nullopt;
        }
        if (parse_register(token).has_value()) {
            add_diag(state, raw.file, raw.line, "registers are not available in .consts: " + token);
            return std::nullopt;
        }
        if (state.aliases.contains(token)) {
            add_diag(state, raw.file, raw.line,
                     "runtime alias is not available in .consts: " + token);
            return std::nullopt;
        }
        return Operand{OperandKind::Register, const_slot(const_program, state, token), 0.0F};
    }

    if (parse_register(token).has_value()) {
        add_diag(state, raw.file, raw.line, "registers are not available in .consts: " + token);
        return std::nullopt;
    }

    if (state.aliases.contains(token)) {
        add_diag(state, raw.file, raw.line, "runtime alias is not available in .consts: " + token);
        return std::nullopt;
    }

    if (const auto slot = const_program.slots.find(token); slot != const_program.slots.end()) {
        return Operand{OperandKind::Register, slot->second, 0.0F};
    }

    if (const auto constant = state.constants.find(token); constant != state.constants.end()) {
        return Operand{OperandKind::Immediate, 0, constant->second};
    }

    if (const auto value = parse_float(token); value.has_value()) {
        return Operand{OperandKind::Immediate, 0, *value};
    }

    add_diag(state, raw.file, raw.line, "invalid const operand: " + token);
    return std::nullopt;
}

ConstProgram lower_const_program(ParseState& state, const ConstBlock& block) {
    ConstProgram const_program;
    predeclare_const_slots(const_program, state, block);

    for (const RawInstruction& raw : block.raw) {
        const auto parsed_op = parse_op(raw.op);
        if (!parsed_op.has_value()) {
            add_diag(state, raw.file, raw.line, "unknown instruction in .consts: " + raw.op);
            continue;
        }

        const Op op = *parsed_op;
        if (!op_allowed_in_consts(op)) {
            add_diag(state, raw.file, raw.line,
                     "runtime-only instruction is not available in .consts: " + raw.op);
            continue;
        }

        const int expected = expected_operands(op);
        if (static_cast<int>(raw.args.size()) != expected) {
            add_diag(state, raw.file, raw.line,
                     raw.op + " expects " + std::to_string(expected) + " operands");
            continue;
        }

        Instruction instruction;
        instruction.op = op;
        instruction.operand_count = expected;
        instruction.line = raw.line;
        instruction.file = raw.file;

        bool failed = false;
        for (int i = 0; i < expected; ++i) {
            const auto operand = parse_const_operand(state, const_program, block, raw, op, i);
            if (!operand.has_value()) {
                failed = true;
                break;
            }
            instruction.operands[static_cast<std::size_t>(i)] = *operand;
        }
        if (!failed) {
            const_program.program.code.push_back(std::move(instruction));
        }
    }

    return const_program;
}

struct ConstEnv {
    std::vector<float>& slots;
    std::vector<bool>& assigned;

    [[nodiscard]] float read(const Operand& operand) const {
        if (operand.kind == OperandKind::Register) {
            return slots[static_cast<std::size_t>(operand.reg)];
        }
        return operand.value;
    }

    void write(const Operand& operand, float value) {
        slots[static_cast<std::size_t>(operand.reg)] = value;
        assigned[static_cast<std::size_t>(operand.reg)] = true;
    }

    void output_unorm(float, float, float, float) {
    }
    void output_byte(float, float, float, float) {
    }

    [[nodiscard]] Rgba sample_channel(int, float, float) const {
        return Rgba{};
    }

    [[nodiscard]] Rgba sample_channel_texel(int, int, int) const {
        return Rgba{};
    }
};

void execute_const_block(ParseState& state, const ConstBlock& block, const std::string& file) {
    const std::size_t diagnostic_count = state.diagnostics.size();
    ConstProgram const_program = lower_const_program(state, block);
    if (state.diagnostics.size() != diagnostic_count) {
        return;
    }

    ConstEnv env{const_program.slot_values, const_program.assigned};
    const detail::StopReason stop =
        detail::execute_program(const_program.program, env, RunLimits{});
    switch (stop) {
    case detail::StopReason::Halted:
    case detail::StopReason::PcOutOfRange:
        break;
    case detail::StopReason::MaxSteps:
        add_diag(state, file, block.start_line, ".consts exceeded max steps");
        return;
    case detail::StopReason::CallDepthExceeded:
        add_diag(state, file, block.start_line, ".consts exceeded max call depth");
        return;
    }

    for (std::size_t i = 0; i < const_program.slot_names.size(); ++i) {
        if (const_program.assigned[i]) {
            state.constants[const_program.slot_names[i]] = const_program.slot_values[i];
        }
    }
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
        instruction.operand_count = expected;
        instruction.line = raw.line;
        instruction.file = raw.file;

        bool failed = false;
        for (int i = 0; i < expected; ++i) {
            const auto operand = parse_operand(state, raw, op, i);
            if (!operand.has_value()) {
                failed = true;
                break;
            }
            instruction.operands[static_cast<std::size_t>(i)] = *operand;
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
    seed_builtin_aliases(state);
    parse_source(state, source, file_name, std::filesystem::current_path(), false);
    AssembleResult result;
    result.program = lower_program(state);
    result.diagnostics = std::move(state.diagnostics);
    result.dependencies = std::move(state.dependencies);
    return result;
}

AssembleResult assemble_file(const std::filesystem::path& path) {
    ParseState state;
    seed_builtin_aliases(state);
    const std::filesystem::path canonical =
        std::filesystem::weakly_canonical(path).lexically_normal();
    std::ifstream input(canonical);
    if (!input) {
        return AssembleResult{
            Program{}, {Diagnostic{canonical.string(), 0, "could not open file"}}, {canonical}};
    }

    state.dependencies.push_back(canonical);
    state.included_files.insert(canonical);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    state.include_stack.insert(canonical);
    parse_source(state, buffer.str(), canonical.string(), canonical.parent_path(), false);

    AssembleResult result;
    result.program = lower_program(state);
    result.diagnostics = std::move(state.diagnostics);
    result.dependencies = std::move(state.dependencies);
    return result;
}

} // namespace ast
