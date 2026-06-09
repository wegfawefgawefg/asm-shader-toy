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

Includes are textual and relative to the file containing the include.

```asm
.include "palette.asm"
```

Recursive includes are rejected.

## Constants

Constants are file-global after parsing.

```asm
.const tau 6.2831853
mul r8, r2, tau
```

## Aliases

Built-in input aliases can be used as source operands:

```asm
norm r16, px, width
add r17, time, frame
```

Input aliases are read-only. User aliases may name scratch registers:

```asm
.alias u, r16
.alias color_r, r17
norm u, px, width
mov color_r, u
```

User aliases may only target `r16..r63`.

## Labels

Labels define branch targets.

```asm
start:
    add r8, r8, 1
    lt r9, r8, 8
    jnz r9, start
```

Programs are bounded by a runtime `--max-steps` limit so accidental infinite
loops do not hang a frame forever.

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
out r, g, b, a      ; normalized 0..1 channels
out8 r, g, b, a     ; byte 0..255 channels
tex dr, dg, db, da, channel, u, v
ret
```

`out` does not stop execution. Use `ret` if the program should end immediately
after writing a color.

`tex` samples one of the four image channels loaded by `--channel0` through
`--channel3`. Coordinates are normalized `0..1`, clamped, nearest-neighbor, and
returned as normalized float channels in the four destination registers.
