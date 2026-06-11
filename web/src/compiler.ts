export type CompileDiagnostic = {
  file: string;
  line: number;
  message: string;
};

export type CompileResult = {
  wgsl: string;
  diagnostics: CompileDiagnostic[];
};

type Operand = { kind: "register"; reg: number } | { kind: "immediate"; value: number };

type Instruction = {
  op: string;
  operands: Operand[];
  file: string;
  line: number;
};

type RawInstruction = {
  op: string;
  args: string[];
  file: string;
  line: number;
};

type ProjectSource = {
  path: string;
  content: string;
};

const builtinAliases = new Map<string, number>([
  ["px", 0],
  ["py", 1],
  ["time", 2],
  ["width", 3],
  ["height", 4],
  ["mouse_x", 5],
  ["mouse_y", 6],
  ["mouse_down", 7],
  ["mouse_click_x", 8],
  ["mouse_click_y", 9],
  ["frame", 10],
  ["time_delta", 11],
  ["wall_clock_seconds", 12],
  ["year", 13],
  ["month", 14],
  ["day", 15]
]);

const stdAliases = new Map<string, number>([
  ["uv_x", 16],
  ["uv_y", 17],
  ["pos_x", 18],
  ["pos_y", 19],
  ["color_r", 20],
  ["color_g", 21],
  ["color_b", 22],
  ["color_a", 23],
  ["tex0_r", 24],
  ["tex0_g", 25],
  ["tex0_b", 26],
  ["tex0_a", 27],
  ["tex1_r", 28],
  ["tex1_g", 29],
  ["tex1_b", 30],
  ["tex1_a", 31],
  ["tmp0", 32],
  ["tmp1", 33],
  ["tmp2", 34],
  ["tmp3", 35],
  ["tmp4", 36],
  ["tmp5", 37],
  ["tmp6", 38],
  ["tmp7", 39],
  ["tmp8", 40],
  ["tmp9", 41],
  ["tmp10", 42],
  ["tmp11", 43],
  ["tmp12", 44],
  ["tmp13", 45],
  ["tmp14", 46],
  ["tmp15", 47]
]);

const screenInstructions: RawInstruction[] = [
  { op: "norm", args: ["uv_x", "px", "width"], file: "<std/screen.inc>", line: 3 },
  { op: "norm", args: ["uv_y", "py", "height"], file: "<std/screen.inc>", line: 4 },
  { op: "mul", args: ["pos_x", "uv_x", "2.0"], file: "<std/screen.inc>", line: 5 },
  { op: "sub", args: ["pos_x", "pos_x", "1.0"], file: "<std/screen.inc>", line: 6 },
  { op: "mul", args: ["pos_y", "uv_y", "2.0"], file: "<std/screen.inc>", line: 7 },
  { op: "sub", args: ["pos_y", "pos_y", "1.0"], file: "<std/screen.inc>", line: 8 }
];

const operandCounts = new Map<string, number>([
  ["ret", 0],
  ["halt", 0],
  ["jmp", 1],
  ["call", 1],
  ["mov", 2],
  ["sin", 2],
  ["cos", 2],
  ["sqrt", 2],
  ["abs", 2],
  ["floor", 2],
  ["fract", 2],
  ["jnz", 2],
  ["jz", 2],
  ["chtime", 2],
  ["chsrate", 2],
  ["key", 2],
  ["mbtn", 2],
  ["mwheel", 2],
  ["gbtn", 2],
  ["gaxis", 2],
  ["add", 3],
  ["sub", 3],
  ["mul", 3],
  ["div", 3],
  ["min", 3],
  ["max", 3],
  ["mod", 3],
  ["norm", 3],
  ["lt", 3],
  ["gt", 3],
  ["eq", 3],
  ["jeq", 3],
  ["jne", 3],
  ["jlt", 3],
  ["jle", 3],
  ["jgt", 3],
  ["jge", 3],
  ["chdim", 3],
  ["out", 4],
  ["out8", 4],
  ["tex", 7],
  ["texel", 7]
]);

const branchTargets = new Map<string, number[]>([
  ["jmp", [0]],
  ["call", [0]],
  ["jnz", [1]],
  ["jz", [1]],
  ["jeq", [2]],
  ["jne", [2]],
  ["jlt", [2]],
  ["jle", [2]],
  ["jgt", [2]],
  ["jge", [2]]
]);

const destinationOperands = new Map<string, number[]>([
  ["mov", [0]],
  ["add", [0]],
  ["sub", [0]],
  ["mul", [0]],
  ["div", [0]],
  ["sin", [0]],
  ["cos", [0]],
  ["sqrt", [0]],
  ["abs", [0]],
  ["floor", [0]],
  ["fract", [0]],
  ["min", [0]],
  ["max", [0]],
  ["mod", [0]],
  ["norm", [0]],
  ["lt", [0]],
  ["gt", [0]],
  ["eq", [0]],
  ["tex", [0, 1, 2, 3]],
  ["texel", [0, 1, 2, 3]],
  ["chdim", [0, 1]],
  ["chtime", [0]],
  ["chsrate", [0]],
  ["key", [0]],
  ["mbtn", [0]],
  ["mwheel", [0, 1]],
  ["gbtn", [0]],
  ["gaxis", [0]]
]);

const emittedOps = new Set([
  "mov",
  "add",
  "sub",
  "mul",
  "div",
  "sin",
  "cos",
  "sqrt",
  "abs",
  "floor",
  "fract",
  "min",
  "max",
  "mod",
  "norm",
  "lt",
  "gt",
  "eq",
  "jmp",
  "jnz",
  "jz",
  "jeq",
  "jne",
  "jlt",
  "jle",
  "jgt",
  "jge",
  "call",
  "ret",
  "tex",
  "texel",
  "chdim",
  "chtime",
  "chsrate",
  "key",
  "mbtn",
  "mwheel",
  "gbtn",
  "gaxis",
  "out",
  "out8",
  "halt"
]);

const constAllowedOps = new Set([
  "mov",
  "add",
  "sub",
  "mul",
  "div",
  "sin",
  "cos",
  "sqrt",
  "abs",
  "floor",
  "fract",
  "min",
  "max",
  "mod",
  "norm",
  "lt",
  "gt",
  "eq",
  "jmp",
  "jnz",
  "jz",
  "jeq",
  "jne",
  "jlt",
  "jle",
  "jgt",
  "jge",
  "call",
  "ret",
  "halt"
]);

function addDiagnostic(diagnostics: CompileDiagnostic[], file: string, line: number, message: string): void {
  diagnostics.push({ file, line, message });
}

function stripComment(line: string): string {
  return line.split(";")[0].trim();
}

function splitArgs(text: string): string[] {
  return text
    .split(",")
    .map((part) => part.trim())
    .filter(Boolean);
}

function normalizePath(base: string, includePath: string): string {
  if (includePath.startsWith("<") && includePath.endsWith(">")) {
    return includePath.slice(1, -1);
  }
  if (includePath.startsWith('"') && includePath.endsWith('"')) {
    const raw = includePath.slice(1, -1);
    const slash = base.lastIndexOf("/");
    return slash >= 0 ? `${base.slice(0, slash + 1)}${raw}` : raw;
  }
  return includePath;
}

function safeDiv(a: number, b: number): number {
  return Math.abs(b) <= 0.000001 ? 0 : a / b;
}

function executeConstBlock(
  block: RawInstruction[],
  labels: Map<string, number>,
  constants: Map<string, number>,
  diagnostics: CompileDiagnostic[]
): void {
  const slots = new Map<string, number>();
  const slotNames: string[] = [];
  const values: number[] = [];
  const assigned: boolean[] = [];

  const slotFor = (name: string): number => {
    const existing = slots.get(name);
    if (existing !== undefined) {
      return existing;
    }
    const index = slotNames.length;
    slots.set(name, index);
    slotNames.push(name);
    values.push(constants.get(name) ?? 0);
    assigned.push(false);
    return index;
  };

  for (const raw of block) {
    const destinations = destinationOperands.get(raw.op) ?? [];
    for (const index of destinations) {
      const token = raw.args[index];
      if (token && !Number.isFinite(Number(token)) && !/^r\d+$/.test(token) && !builtinAliases.has(token)) {
        slotFor(token);
      }
    }
  }

  const valueOf = (raw: RawInstruction, token: string): number => {
    const slot = slots.get(token);
    if (slot !== undefined) {
      return values[slot];
    }
    const constant = constants.get(token);
    if (constant !== undefined) {
      return constant;
    }
    const value = Number(token);
    if (Number.isFinite(value)) {
      return value;
    }
    addDiagnostic(diagnostics, raw.file, raw.line, `invalid const operand: ${token}`);
    return 0;
  };

  const writeSlot = (raw: RawInstruction, token: string, value: number): void => {
    if (Number.isFinite(Number(token)) || /^r\d+$/.test(token) || builtinAliases.has(token)) {
      addDiagnostic(diagnostics, raw.file, raw.line, `const destination must be a name: ${token}`);
      return;
    }
    const slot = slotFor(token);
    values[slot] = value;
    assigned[slot] = true;
  };

  let pc = 0;
  let steps = 0;
  const callStack: number[] = [];
  while (pc >= 0 && pc < block.length) {
    if (steps >= 4096) {
      addDiagnostic(diagnostics, block[0]?.file ?? "", block[0]?.line ?? 0, ".consts exceeded max steps");
      return;
    }
    steps += 1;
    const raw = block[pc];
    pc += 1;
    const a = (index: number) => valueOf(raw, raw.args[index]);
    const jump = (index: number): void => {
      const target = labels.get(raw.args[index]);
      if (target === undefined) {
        addDiagnostic(diagnostics, raw.file, raw.line, `unknown label: ${raw.args[index]}`);
        pc = block.length;
      } else {
        pc = target;
      }
    };

    switch (raw.op) {
      case "mov":
        writeSlot(raw, raw.args[0], a(1));
        break;
      case "add":
        writeSlot(raw, raw.args[0], a(1) + a(2));
        break;
      case "sub":
        writeSlot(raw, raw.args[0], a(1) - a(2));
        break;
      case "mul":
        writeSlot(raw, raw.args[0], a(1) * a(2));
        break;
      case "div":
      case "norm":
        writeSlot(raw, raw.args[0], safeDiv(a(1), a(2)));
        break;
      case "sin":
        writeSlot(raw, raw.args[0], Math.sin(a(1)));
        break;
      case "cos":
        writeSlot(raw, raw.args[0], Math.cos(a(1)));
        break;
      case "sqrt":
        writeSlot(raw, raw.args[0], Math.sqrt(Math.max(0, a(1))));
        break;
      case "abs":
        writeSlot(raw, raw.args[0], Math.abs(a(1)));
        break;
      case "floor":
        writeSlot(raw, raw.args[0], Math.floor(a(1)));
        break;
      case "fract":
        writeSlot(raw, raw.args[0], a(1) - Math.floor(a(1)));
        break;
      case "min":
        writeSlot(raw, raw.args[0], Math.min(a(1), a(2)));
        break;
      case "max":
        writeSlot(raw, raw.args[0], Math.max(a(1), a(2)));
        break;
      case "mod":
        writeSlot(raw, raw.args[0], a(1) % a(2));
        break;
      case "lt":
        writeSlot(raw, raw.args[0], a(1) < a(2) ? 1 : 0);
        break;
      case "gt":
        writeSlot(raw, raw.args[0], a(1) > a(2) ? 1 : 0);
        break;
      case "eq":
        writeSlot(raw, raw.args[0], Math.abs(a(1) - a(2)) <= 0.000001 ? 1 : 0);
        break;
      case "jmp":
        jump(0);
        break;
      case "jnz":
        if (a(0) !== 0) jump(1);
        break;
      case "jz":
        if (a(0) === 0) jump(1);
        break;
      case "jeq":
        if (Math.abs(a(0) - a(1)) <= 0.000001) jump(2);
        break;
      case "jne":
        if (Math.abs(a(0) - a(1)) > 0.000001) jump(2);
        break;
      case "jlt":
        if (a(0) < a(1)) jump(2);
        break;
      case "jle":
        if (a(0) <= a(1)) jump(2);
        break;
      case "jgt":
        if (a(0) > a(1)) jump(2);
        break;
      case "jge":
        if (a(0) >= a(1)) jump(2);
        break;
      case "call": {
        const target = labels.get(raw.args[0]);
        if (target === undefined) {
          addDiagnostic(diagnostics, raw.file, raw.line, `unknown label: ${raw.args[0]}`);
          pc = block.length;
          break;
        }
        if (callStack.length >= 32) {
          addDiagnostic(diagnostics, raw.file, raw.line, ".consts exceeded max call depth");
          pc = block.length;
          break;
        }
        callStack.push(pc);
        pc = target;
        break;
      }
      case "ret":
        pc = callStack.pop() ?? block.length;
        break;
      case "halt":
        pc = block.length;
        break;
      default:
        addDiagnostic(diagnostics, raw.file, raw.line, `runtime-only instruction is not available in .consts: ${raw.op}`);
        pc = block.length;
        break;
    }
  }

  for (let index = 0; index < slotNames.length; ++index) {
    if (assigned[index]) {
      constants.set(slotNames[index], values[index]);
    }
  }
}

function parseSource(
  files: ProjectSource[],
  path: string,
  aliases: Map<string, number>,
  constants: Map<string, number>,
  raw: RawInstruction[],
  labels: Map<string, number>,
  diagnostics: CompileDiagnostic[],
  included: Set<string>
): void {
  if (included.has(path)) {
    return;
  }
  included.add(path);

  if (path === "std/aliases.inc") {
    for (const [name, reg] of stdAliases) {
      aliases.set(name, reg);
    }
    return;
  }
  if (path === "std/screen.inc") {
    parseSource(files, "std/aliases.inc", aliases, constants, raw, labels, diagnostics, included);
    raw.push(...screenInstructions);
    return;
  }

  const file = files.find((candidate) => candidate.path === path);
  if (!file) {
    addDiagnostic(diagnostics, path, 0, "could not open include");
    return;
  }

  const lines = file.content.split(/\r?\n/);
  for (let index = 0; index < lines.length; ++index) {
    const lineNumber = index + 1;
    const line = stripComment(lines[index]);
    if (!line) {
      continue;
    }
    if (line.startsWith(".include")) {
      const includePath = line.slice(".include".length).trim();
      parseSource(files, normalizePath(path, includePath), aliases, constants, raw, labels, diagnostics, included);
      continue;
    }
    if (line.startsWith(".consts")) {
      const block: RawInstruction[] = [];
      const blockLabels = new Map<string, number>();
      for (++index; index < lines.length; ++index) {
        const blockLineNumber = index + 1;
        const blockLine = stripComment(lines[index]);
        if (!blockLine) {
          continue;
        }
        if (blockLine === ".end") {
          break;
        }
        if (blockLine.endsWith(":")) {
          blockLabels.set(blockLine.slice(0, -1), block.length);
          continue;
        }
        const split = blockLine.search(/\s/);
        const op = (split >= 0 ? blockLine.slice(0, split) : blockLine).toLowerCase();
        const args = split >= 0 ? splitArgs(blockLine.slice(split + 1)) : [];
        const expected = operandCounts.get(op);
        if (expected === undefined || !constAllowedOps.has(op)) {
          addDiagnostic(diagnostics, path, blockLineNumber, `runtime-only instruction is not available in .consts: ${op}`);
        } else if (args.length !== expected) {
          addDiagnostic(diagnostics, path, blockLineNumber, `${op} expects ${expected} operands`);
        }
        block.push({ op, args, file: path, line: blockLineNumber });
      }
      if (index >= lines.length) {
        addDiagnostic(diagnostics, path, lineNumber, ".consts missing .end");
      }
      executeConstBlock(block, blockLabels, constants, diagnostics);
      continue;
    }
    if (line.startsWith(".const")) {
      const parts = line.split(/\s+/);
      const value = Number(parts[2]);
      if (parts.length !== 3 || !Number.isFinite(value)) {
        addDiagnostic(diagnostics, path, lineNumber, ".const expects name and value");
      } else {
        constants.set(parts[1], value);
      }
      continue;
    }
    if (line.startsWith(".alias")) {
      const parts = line.split(/[,\s]+/).filter(Boolean);
      const match = /^r(\d+)$/.exec(parts[2] ?? "");
      if (parts.length !== 3 || !match) {
        addDiagnostic(diagnostics, path, lineNumber, "invalid alias");
        continue;
      }
      aliases.set(parts[1], Number(match[1]));
      continue;
    }
    if (line.endsWith(":")) {
      labels.set(line.slice(0, -1), raw.length);
      continue;
    }
    const split = line.search(/\s/);
    const op = (split >= 0 ? line.slice(0, split) : line).toLowerCase();
    const args = split >= 0 ? splitArgs(line.slice(split + 1)) : [];
    raw.push({ op, args, file: path, line: lineNumber });
  }
}

function parseOperand(
  token: string,
  instruction: RawInstruction,
  operandIndex: number,
  aliases: Map<string, number>,
  constants: Map<string, number>,
  labels: Map<string, number>,
  diagnostics: CompileDiagnostic[]
): Operand {
  if (branchTargets.get(instruction.op)?.includes(operandIndex)) {
    const target = labels.get(token);
    if (target === undefined) {
      addDiagnostic(diagnostics, instruction.file, instruction.line, `unknown label: ${token}`);
      return { kind: "immediate", value: 0 };
    }
    return { kind: "immediate", value: target };
  }

  const register = aliases.get(token);
  if (register !== undefined) {
    return { kind: "register", reg: register };
  }
  const constant = constants.get(token);
  if (constant !== undefined) {
    return { kind: "immediate", value: constant };
  }
  const rawRegister = /^r(\d+)$/.exec(token);
  if (rawRegister) {
    return { kind: "register", reg: Number(rawRegister[1]) };
  }
  const value = Number(token);
  if (!Number.isFinite(value)) {
    addDiagnostic(diagnostics, instruction.file, instruction.line, `invalid operand: ${token}`);
    return { kind: "immediate", value: 0 };
  }
  return { kind: "immediate", value };
}

function assemble(files: ProjectSource[], main: string): { instructions: Instruction[]; diagnostics: CompileDiagnostic[] } {
  const aliases = new Map(builtinAliases);
  const constants = new Map<string, number>();
  const raw: RawInstruction[] = [];
  const labels = new Map<string, number>();
  const diagnostics: CompileDiagnostic[] = [];
  parseSource(files, main, aliases, constants, raw, labels, diagnostics, new Set());

  const instructions = raw.map((instruction) => {
    const expected = operandCounts.get(instruction.op);
    if (expected === undefined) {
      addDiagnostic(diagnostics, instruction.file, instruction.line, `unknown op: ${instruction.op}`);
    } else if (instruction.args.length !== expected) {
      addDiagnostic(diagnostics, instruction.file, instruction.line, `${instruction.op} expects ${expected} operands`);
    }
    const destinations = destinationOperands.get(instruction.op) ?? [];
    const operands = instruction.args.map((token, index) =>
      parseOperand(token, instruction, index, aliases, constants, labels, diagnostics)
    );
    for (const index of destinations) {
      if (operands[index]?.kind !== "register") {
        addDiagnostic(diagnostics, instruction.file, instruction.line, "destination must be a register");
      }
    }
    return { op: instruction.op, operands, file: instruction.file, line: instruction.line };
  });

  return { instructions, diagnostics };
}

function wgslFloat(value: number): string {
  return Number.isInteger(value) ? `${value}.0` : String(value);
}

function readOperand(operand: Operand): string {
  return operand.kind === "register" ? `r[${operand.reg}]` : wgslFloat(operand.value);
}

function writeRegister(operand: Operand, expression: string): string {
  return operand.kind === "register" ? `r[${operand.reg}] = ${expression};` : "";
}

function target(operand: Operand): number {
  return operand.kind === "immediate" ? operand.value : -1;
}

function header(maxSteps: number): string {
  return `struct AstInputs {
    time: f32,
    time_delta: f32,
    frame: f32,
    width: f32,
    height: f32,
    mouse_x: f32,
    mouse_y: f32,
    mouse_down: f32,
    mouse_click_x: f32,
    mouse_click_y: f32,
    wall_clock_seconds: f32,
    year: f32,
    month: f32,
    day: f32,
    channel0: vec4<f32>,
    channel1: vec4<f32>,
    channel2: vec4<f32>,
    channel3: vec4<f32>,
    keys: array<vec4<f32>, 128>,
    mouse_buttons: array<vec4<f32>, 2>,
    mouse_wheel: vec4<f32>,
    gamepad_buttons: array<vec4<f32>, 8>,
    gamepad_axes: array<vec4<f32>, 4>,
};

@group(0) @binding(0) var output_texture: texture_storage_2d<rgba8unorm, write>;
@group(0) @binding(1) var<uniform> ast_inputs: AstInputs;
@group(0) @binding(2) var channel0_texture: texture_2d<f32>;
@group(0) @binding(3) var channel1_texture: texture_2d<f32>;
@group(0) @binding(4) var channel2_texture: texture_2d<f32>;
@group(0) @binding(5) var channel3_texture: texture_2d<f32>;

fn ast_safe_div(a: f32, b: f32) -> f32 { if (abs(b) <= 0.000001) { return 0.0; } return a / b; }
fn ast_mod(a: f32, b: f32) -> f32 { if (abs(b) <= 0.000001) { return 0.0; } return a - floor(a / b) * b; }
fn ast_eq(a: f32, b: f32) -> bool { return abs(a - b) <= 0.000001; }
fn ast_unorm(v: f32) -> f32 { return clamp(v, 0.0, 1.0); }
fn ast_byte(v: f32) -> f32 { return clamp(v, 0.0, 255.0) / 255.0; }
fn ast_channel_meta(channel: i32) -> vec4<f32> {
    switch (channel) {
    case 0: { return ast_inputs.channel0; }
    case 1: { return ast_inputs.channel1; }
    case 2: { return ast_inputs.channel2; }
    case 3: { return ast_inputs.channel3; }
    default: { return vec4<f32>(0.0); }
    }
}
fn ast_channel_load(channel: i32, coord: vec2<i32>) -> vec4<f32> {
    switch (channel) {
    case 0: { return textureLoad(channel0_texture, coord, 0); }
    case 1: { return textureLoad(channel1_texture, coord, 0); }
    case 2: { return textureLoad(channel2_texture, coord, 0); }
    case 3: { return textureLoad(channel3_texture, coord, 0); }
    default: { return vec4<f32>(0.0); }
    }
}
fn ast_packed_vec4_read(value: vec4<f32>, lane: i32) -> f32 {
    switch (lane) {
    case 0: { return value.x; }
    case 1: { return value.y; }
    case 2: { return value.z; }
    case 3: { return value.w; }
    default: { return 0.0; }
    }
}
fn ast_key_state(scancode: i32) -> f32 {
    if (scancode < 0 || scancode >= 512) { return 0.0; }
    let bucket = scancode / 4;
    let lane = scancode - bucket * 4;
    return ast_packed_vec4_read(ast_inputs.keys[bucket], lane);
}
fn ast_mouse_button_state(button: i32) -> f32 {
    if (button < 0 || button >= 8) { return 0.0; }
    let bucket = button / 4;
    let lane = button - bucket * 4;
    return ast_packed_vec4_read(ast_inputs.mouse_buttons[bucket], lane);
}
fn ast_gamepad_button_state(button: i32) -> f32 {
    if (button < 0 || button >= 32) { return 0.0; }
    let bucket = button / 4;
    let lane = button - bucket * 4;
    return ast_packed_vec4_read(ast_inputs.gamepad_buttons[bucket], lane);
}
fn ast_gamepad_axis_state(axis: i32) -> f32 {
    if (axis < 0 || axis >= 16) { return 0.0; }
    let bucket = axis / 4;
    let lane = axis - bucket * 4;
    return ast_packed_vec4_read(ast_inputs.gamepad_axes[bucket], lane);
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    if (gid.x >= u32(ast_inputs.width) || gid.y >= u32(ast_inputs.height)) { return; }
    var r: array<f32, 64>;
    r[0] = f32(gid.x); r[1] = f32(gid.y); r[2] = ast_inputs.time; r[3] = ast_inputs.width;
    r[4] = ast_inputs.height; r[5] = ast_inputs.mouse_x; r[6] = ast_inputs.mouse_y;
    r[7] = ast_inputs.mouse_down; r[8] = ast_inputs.mouse_click_x; r[9] = ast_inputs.mouse_click_y;
    r[10] = ast_inputs.frame; r[11] = ast_inputs.time_delta; r[12] = ast_inputs.wall_clock_seconds;
    r[13] = ast_inputs.year; r[14] = ast_inputs.month; r[15] = ast_inputs.day;
    var color = vec4<f32>(0.0, 0.0, 0.0, 1.0);
    var pc: i32 = 0;
    var steps: i32 = 0;
    var call_stack: array<i32, 32>;
    var call_depth: i32 = 0;
    loop {
        if (pc < 0 || steps >= ${maxSteps}) { break; }
        steps = steps + 1;
        switch (pc) {
`;
}

function emitInstruction(instruction: Instruction, pc: number, programSize: number): string {
  const o = instruction.operands;
  const next = pc + 1;
  const line = `            // ${instruction.file}:${instruction.line}\n`;
  switch (instruction.op) {
    case "mov":
      return `${line}            ${writeRegister(o[0], readOperand(o[1]))}\n            pc = ${next};\n`;
    case "add":
      return `${line}            ${writeRegister(o[0], `${readOperand(o[1])} + ${readOperand(o[2])}`)}\n            pc = ${next};\n`;
    case "sub":
      return `${line}            ${writeRegister(o[0], `${readOperand(o[1])} - ${readOperand(o[2])}`)}\n            pc = ${next};\n`;
    case "mul":
      return `${line}            ${writeRegister(o[0], `${readOperand(o[1])} * ${readOperand(o[2])}`)}\n            pc = ${next};\n`;
    case "div":
    case "norm":
      return `${line}            ${writeRegister(o[0], `ast_safe_div(${readOperand(o[1])}, ${readOperand(o[2])})`)}\n            pc = ${next};\n`;
    case "sin":
    case "cos":
    case "abs":
    case "floor":
      return `${line}            ${writeRegister(o[0], `${instruction.op}(${readOperand(o[1])})`)}\n            pc = ${next};\n`;
    case "sqrt":
      return `${line}            ${writeRegister(o[0], `sqrt(max(0.0, ${readOperand(o[1])}))`)}\n            pc = ${next};\n`;
    case "fract":
      return `${line}            ${writeRegister(o[0], `${readOperand(o[1])} - floor(${readOperand(o[1])})`)}\n            pc = ${next};\n`;
    case "min":
    case "max":
      return `${line}            ${writeRegister(o[0], `${instruction.op}(${readOperand(o[1])}, ${readOperand(o[2])})`)}\n            pc = ${next};\n`;
    case "mod":
      return `${line}            ${writeRegister(o[0], `ast_mod(${readOperand(o[1])}, ${readOperand(o[2])})`)}\n            pc = ${next};\n`;
    case "lt":
      return `${line}            ${writeRegister(o[0], `select(0.0, 1.0, ${readOperand(o[1])} < ${readOperand(o[2])})`)}\n            pc = ${next};\n`;
    case "gt":
      return `${line}            ${writeRegister(o[0], `select(0.0, 1.0, ${readOperand(o[1])} > ${readOperand(o[2])})`)}\n            pc = ${next};\n`;
    case "eq":
      return `${line}            ${writeRegister(o[0], `select(0.0, 1.0, ast_eq(${readOperand(o[1])}, ${readOperand(o[2])}))`)}\n            pc = ${next};\n`;
    case "jmp":
      return `${line}            pc = ${target(o[0])};\n`;
    case "jnz":
      return `${line}            if (${readOperand(o[0])} != 0.0) { pc = ${target(o[1])}; } else { pc = ${next}; }\n`;
    case "jz":
      return `${line}            if (${readOperand(o[0])} == 0.0) { pc = ${target(o[1])}; } else { pc = ${next}; }\n`;
    case "jlt":
    case "jle":
    case "jgt":
    case "jge":
    case "jeq":
    case "jne": {
      const op = new Map([
        ["jlt", "<"],
        ["jle", "<="],
        ["jgt", ">"],
        ["jge", ">="]
      ]).get(instruction.op);
      const condition =
        instruction.op === "jeq"
          ? `ast_eq(${readOperand(o[0])}, ${readOperand(o[1])})`
          : instruction.op === "jne"
            ? `!ast_eq(${readOperand(o[0])}, ${readOperand(o[1])})`
            : `${readOperand(o[0])} ${op} ${readOperand(o[1])}`;
      return `${line}            if (${condition}) { pc = ${target(o[2])}; } else { pc = ${next}; }\n`;
    }
    case "call":
      return `${line}            if (call_depth >= 32) { pc = -1; } else { call_stack[call_depth] = ${next}; call_depth = call_depth + 1; pc = ${target(o[0])}; }\n`;
    case "ret":
      return `${line}            if (call_depth > 0) { call_depth = call_depth - 1; pc = call_stack[call_depth]; } else { pc = ${programSize}; }\n`;
    case "tex":
      return `${line}            let tex_channel_${pc} = i32(${readOperand(o[4])});
            let tex_meta_${pc} = ast_channel_meta(tex_channel_${pc});
            if (tex_meta_${pc}.x <= 0.0 || tex_meta_${pc}.y <= 0.0) {
                ${writeRegister(o[0], "0.0")} ${writeRegister(o[1], "0.0")} ${writeRegister(o[2], "0.0")} ${writeRegister(o[3], "0.0")}
            } else {
                let tex_w_${pc} = i32(tex_meta_${pc}.x);
                let tex_h_${pc} = i32(tex_meta_${pc}.y);
                let tex_x_${pc} = clamp(i32(floor(clamp(${readOperand(o[5])}, 0.0, 1.0) * tex_meta_${pc}.x)), 0, tex_w_${pc} - 1);
                let tex_y_${pc} = clamp(i32(floor(clamp(${readOperand(o[6])}, 0.0, 1.0) * tex_meta_${pc}.y)), 0, tex_h_${pc} - 1);
                let tex_sample_${pc} = ast_channel_load(tex_channel_${pc}, vec2<i32>(tex_x_${pc}, tex_y_${pc}));
                ${writeRegister(o[0], `tex_sample_${pc}.r`)} ${writeRegister(o[1], `tex_sample_${pc}.g`)} ${writeRegister(o[2], `tex_sample_${pc}.b`)} ${writeRegister(o[3], `tex_sample_${pc}.a`)}
            }
            pc = ${next};\n`;
    case "texel":
      return `${line}            let texel_channel_${pc} = i32(${readOperand(o[4])});
            let texel_meta_${pc} = ast_channel_meta(texel_channel_${pc});
            let texel_x_${pc} = i32(round(${readOperand(o[5])}));
            let texel_y_${pc} = i32(round(${readOperand(o[6])}));
            if (texel_meta_${pc}.x <= 0.0 || texel_meta_${pc}.y <= 0.0 || texel_x_${pc} < 0 || texel_y_${pc} < 0 || texel_x_${pc} >= i32(texel_meta_${pc}.x) || texel_y_${pc} >= i32(texel_meta_${pc}.y)) {
                ${writeRegister(o[0], "0.0")} ${writeRegister(o[1], "0.0")} ${writeRegister(o[2], "0.0")} ${writeRegister(o[3], "0.0")}
            } else {
                let texel_sample_${pc} = ast_channel_load(texel_channel_${pc}, vec2<i32>(texel_x_${pc}, texel_y_${pc}));
                ${writeRegister(o[0], `texel_sample_${pc}.r`)} ${writeRegister(o[1], `texel_sample_${pc}.g`)} ${writeRegister(o[2], `texel_sample_${pc}.b`)} ${writeRegister(o[3], `texel_sample_${pc}.a`)}
            }
            pc = ${next};\n`;
    case "chdim":
      return `${line}            let chdim_meta_${pc} = ast_channel_meta(i32(${readOperand(o[2])}));
            ${writeRegister(o[0], `chdim_meta_${pc}.x`)}
            ${writeRegister(o[1], `chdim_meta_${pc}.y`)}
            pc = ${next};\n`;
    case "chtime":
      return `${line}            let chtime_meta_${pc} = ast_channel_meta(i32(${readOperand(o[1])}));
            ${writeRegister(o[0], `chtime_meta_${pc}.z`)}
            pc = ${next};\n`;
    case "chsrate":
      return `${line}            let chsrate_meta_${pc} = ast_channel_meta(i32(${readOperand(o[1])}));
            ${writeRegister(o[0], `chsrate_meta_${pc}.w`)}
            pc = ${next};\n`;
    case "key":
      return `${line}            ${writeRegister(o[0], `ast_key_state(i32(${readOperand(o[1])}))`)}
            pc = ${next};\n`;
    case "mbtn":
      return `${line}            ${writeRegister(o[0], `ast_mouse_button_state(i32(${readOperand(o[1])}))`)}
            pc = ${next};\n`;
    case "mwheel":
      return `${line}            ${writeRegister(o[0], "ast_inputs.mouse_wheel.x")}
            ${writeRegister(o[1], "ast_inputs.mouse_wheel.y")}
            pc = ${next};\n`;
    case "gbtn":
      return `${line}            ${writeRegister(o[0], `ast_gamepad_button_state(i32(${readOperand(o[1])}))`)}
            pc = ${next};\n`;
    case "gaxis":
      return `${line}            ${writeRegister(o[0], `ast_gamepad_axis_state(i32(${readOperand(o[1])}))`)}
            pc = ${next};\n`;
    case "out":
      return `${line}            color = vec4<f32>(ast_unorm(${readOperand(o[0])}), ast_unorm(${readOperand(o[1])}), ast_unorm(${readOperand(o[2])}), ast_unorm(${readOperand(o[3])}));\n            pc = ${next};\n`;
    case "out8":
      return `${line}            color = vec4<f32>(ast_byte(${readOperand(o[0])}), ast_byte(${readOperand(o[1])}), ast_byte(${readOperand(o[2])}), ast_byte(${readOperand(o[3])}));\n            pc = ${next};\n`;
    case "halt":
      return `${line}            pc = ${programSize};\n`;
    default:
      return `${line}            pc = -1;\n`;
  }
}

export function compileAsmToWgsl(files: ProjectSource[], main: string, maxSteps = 4096): CompileResult {
  const { instructions, diagnostics } = assemble(files, main);
  for (const instruction of instructions) {
    if (!emittedOps.has(instruction.op)) {
      addDiagnostic(diagnostics, instruction.file, instruction.line, `opcode '${instruction.op}' is not supported by the browser compiler yet`);
    }
  }
  if (diagnostics.length > 0) {
    return { wgsl: "", diagnostics };
  }
  let wgsl = header(maxSteps);
  instructions.forEach((instruction, pc) => {
    wgsl += `        case ${pc}: {\n`;
    wgsl += emitInstruction(instruction, pc, instructions.length);
    wgsl += "        }\n";
  });
  wgsl += `        default: { pc = -1; }
        }
    }
    textureStore(output_texture, vec2<i32>(i32(gid.x), i32(gid.y)), color);
}
`;
  return { wgsl, diagnostics: [] };
}
