# Assembly

The language is fake assembly for per-pixel image programs. It is intentionally
small and shader-shaped.

## Comments

These comment forms are accepted:

```asm
; comment
# comment
// comment
```

## Includes

Includes are relative to the file containing the include. Standard includes use
angle brackets and resolve under `stdlib/`.

```asm
.include "palette.asm"
.include <std/screen.inc>
```

Includes are once-by-default. The assembler resolves each include to a canonical
path and parses it only once per program, so shared files can safely include the
same common helper. Recursive includes are rejected.

## Constants

Constants are file-global after parsing.

```asm
.const tau 6.2831853
mul r8, r2, tau
```

Assembler-time constant blocks run a small compile-time program and export
assigned slots as constants:

```asm
.consts
    mov pi, 3.14159265
    add tau, pi, pi
    mul half_tau, tau, 0.5
.end

mul tmp0, time, tau
```

Inside `.consts`, destination names are compile-time slots, not runtime
registers. Runtime inputs such as `px`, `time`, and `width` are unavailable.
Every opcode that does not require runtime state is allowed, including math,
branches, local labels, `call`, `ret`, and `halt`. Runtime-only operations
`tex`, `texel`, `out`, and `out8` are rejected.

## Aliases

Built-in input aliases can be used as source operands:

```asm
norm r16, px, width
add r17, time, frame
```

Input aliases are read-only and reserved. User aliases may name scratch
registers:

```asm
.alias u, r16
.alias color_r, r17
norm u, px, width
mov color_r, u
```

User aliases may only target `r16..r63`. Built-in input aliases and aliases from
standard-library includes cannot be redefined.

For normal programs, prefer the standard include:

```asm
.include <std/screen.inc>
```

It defines conventional scratch aliases:

```text
uv_x, uv_y          normalized pixel coordinates
pos_x, pos_y        centered coordinates, -1..1
color_r..color_a    primary output color
tex0_r..tex0_a      first sampled texture color
tex1_r..tex1_a      second sampled texture color
tmp0..tmp15         general temporaries
```

## Labels

Labels define branch and call targets.

```asm
start:
    add r8, r8, 1
    lt r9, r8, 8
    jnz r9, start
```

Local labels start with `.` and are scoped under the previous global label.
They are useful inside subroutines and helper blocks.

```asm
palette:
    jlt tmp0, 2.0, .blue
    jmp .pink
.blue:
    ret
.pink:
    ret
```

Programs are bounded by a runtime `--max-steps` limit so accidental infinite
loops do not hang a frame forever.

## Subroutines

`call label` pushes a return address onto a small bounded call stack and jumps
to `label`. `ret` returns to the caller when the stack is non-empty. For
backward compatibility, `ret` still halts the program when no caller exists.
Use `halt` for an explicit program stop.

```asm
main:
    call palette
    out color_r, color_g, color_b, 1.0
    halt

palette:
    mov color_r, 1.0
    mov color_g, 0.2
    mov color_b, 0.5
    ret
```

## Registers

There are 64 float registers: `r0` through `r63`.

Initial registers:

- `r0`: pixel x
- `r1`: pixel y
- `r2`: time in seconds
- `r3`: render width
- `r4`: render height
- `r5`: mouse x while left button is down, otherwise `0`
- `r6`: mouse y while left button is down, otherwise `0`
- `r7`: mouse down, `1` or `0`
- `r8`: mouse click x
- `r9`: mouse click y
- `r10`: frame index
- `r11`: previous frame delta time in seconds
- `r12`: local wall-clock seconds since midnight
- `r13`: local year
- `r14`: local month, `1..12`
- `r15`: local day of month, `1..31`

Scratch registers start at `r16`.

Named aliases for input registers:

```text
px              r0
pixel_x         r0
py              r1
pixel_y         r1
time            r2
width           r3
height          r4
mouse_x         r5
mouse_y         r6
mouse_down      r7
mouse_click_x   r8
mouse_click_y   r9
frame           r10
time_delta      r11
wall_seconds    r12
date_year       r13
date_month      r14
date_day        r15
```

## Instructions

Instruction operands may be registers, constants, or numeric immediates unless
the operand is a destination register.

```text
mov dst, a
add dst, a, b
sub dst, a, b
mul dst, a, b
div dst, a, b
norm dst, a, b      ; same as safe a / b
sin dst, a
cos dst, a
sqrt dst, a
abs dst, a
floor dst, a
fract dst, a
min dst, a, b
max dst, a, b
mod dst, a, b
lt dst, a, b        ; 1.0 if a < b, else 0.0
gt dst, a, b
eq dst, a, b
jmp label
jnz test, label
jz test, label
jeq a, b, label
jne a, b, label
jlt a, b, label
jle a, b, label
jgt a, b, label
jge a, b, label
call label
out r, g, b, a      ; normalized 0..1 channels
out8 r, g, b, a     ; byte 0..255 channels
tex dr, dg, db, da, channel, u, v
texel dr, dg, db, da, channel, x, y
ret
halt
```

`out` does not stop execution. Use `halt` if the program should end immediately
after writing a color. `ret` is for returning from `call`, but still halts when
used with an empty call stack.

`tex` samples one of the four channels loaded by `--channel0` through
`--channel3` or produced by `--buffer0` through `--buffer3`. Coordinates are
normalized `0..1`, clamped, nearest-neighbor, and returned as normalized float
channels in the four destination registers.

`texel` samples one of the same channels using pixel coordinates. Out-of-bounds
coordinates return transparent black instead of clamping.

## Feedback Buffers

Buffer passes are extra per-frame programs that render into channels before the
main image program runs:

```sh
./build/asm-shader-toy examples/buffers/life_display.asm \
  --buffer0 examples/buffers/life_buffer.asm
```

Buffer N writes channel N. During buffer rendering, channels contain the
previous frame's buffer contents, so a buffer can sample itself with `tex ..., N,
u, v` to build feedback effects like cellular automata. The image pass sees the
newly rendered buffer channels for the current frame.
