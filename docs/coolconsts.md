# Cool Consts

`.consts ... .end` is assembler-time asm. The goal is to make constants feel
like assembly instead of adding a separate infix expression language.

## Shape

```asm
.consts
    mov pi, 3.14159265
    add tau, pi, pi
    mul half_tau, tau, 0.5
    div inv_255, 1.0, 255.0
.end

mul tmp0, time, tau
```

The `.consts` block runs once while assembling. Names assigned inside the block
become normal constants after `.end`, so the runtime program sees `tau` as an
immediate constant.

## Mental Model

`.consts` is a compile-time program in a different environment:

- Compile-time names such as `pi`, `tau`, and `half_tau` are slots.
- Runtime inputs such as `px`, `time`, `width`, and `mouse_x` are unavailable.
- Runtime channel operations are unavailable.
- The assembler applies max-step and max-call-depth limits.
- After `.end`, assigned slots are exported into the broader constant table.

## Implementation

Do not double-implement the VM.

Opcode behavior lives in one shared executor shape. The runtime path and const
path have different operand resolution and environments, but math, comparison,
branch, call, return, and halt semantics are not copied into a second switch.

The runtime render path stays direct by using an inlineable templated executor:

```cpp
execute_program<RuntimeEnv>(program, runtime_env, limits);
execute_program<ConstEnv>(program, const_env, limits);
```

`RuntimeEnv` should still compile down to direct register array access in the
per-pixel loop. The const environment can use named slots because it does not
run per pixel.

## Environments

Runtime environment:

- Reads operands from registers or immediates.
- Writes destination operands to registers.
- Supports `tex`, `texel`, `out`, and `out8`.
- Receives pixel, frame, mouse, date, and channel inputs.

Const environment:

- Reads operands from named compile-time slots or immediates.
- Writes destination operands to named compile-time slots.
- Supports normal compute/control asm.
- Rejects runtime-only inputs and operations.

## Supported Ops

The intended rule is: `.consts` supports every opcode that does not require
runtime state.

Supported:

- `mov`
- `add`, `sub`, `mul`, `div`, `norm`
- `sin`, `cos`, `sqrt`, `abs`, `floor`, `fract`, `min`, `max`, `mod`
- `lt`, `gt`, `eq`
- `jmp`, `jnz`, `jz`
- `jeq`, `jne`, `jlt`, `jle`, `jgt`, `jge`
- `call`, `ret`, `halt`

Rejected in `.consts`:

- `tex`, because it samples runtime channels
- `texel`, because it samples runtime channels
- `chdim`, because it reads runtime channel metadata
- `chtime`, because it reads runtime channel metadata
- `out`, because there is no runtime pixel output
- `out8`, because there is no runtime pixel output
- runtime input aliases such as `px`, `py`, `time`, `width`, `height`,
  `mouse_x`, and `frame`

If asset-driven compile-time sampling becomes useful later, it should be a
separate deliberate design, not an accidental consequence of `.consts`.

## Parsing And Lowering

`.consts ... .end` parses as a separate compile-time program.

Labels and local labels inside the block belong to that block and do not collide
with runtime labels. The block reuses normal instruction syntax:

```asm
.consts
start:
    mov i, 0
    mov acc, 0
.loop:
    add acc, acc, 0.25
    add i, i, 1
    jlt i, 4, .loop
    mov one, acc
    halt
.end
```

Operand lowering reuses `Instruction` and stores const slots in register-shaped
operands for the const environment. Runtime instruction representation stays
tight, and opcode semantics stay shared.

## Execution

The assembler executes each const block during assembly:

1. Parse the block into a compile-time program.
2. Seed the const environment with constants already known before the block.
3. Run the shared executor with const limits.
4. Export assigned slots into `state.constants`.
5. Continue parsing the outer runtime program.

Later constants and runtime instructions can reference exported names.

## Tests

Useful tests:

- Literal assignment exports a constant.
- Math ops export expected values.
- Constants can reference earlier constants from the same block.
- Constants can reference earlier constants from previous blocks or `.const`.
- Branches and local labels work inside `.consts`.
- `call`, `ret`, and `halt` work inside `.consts`.
- Runtime aliases such as `time` are rejected in `.consts`.
- Runtime-only ops `tex`, `texel`, `out`, and `out8` are rejected in `.consts`.
- Max-step limits prevent infinite compile-time loops.
