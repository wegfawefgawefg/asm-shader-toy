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

There are 32 float registers: `r0` through `r31`.

Initial registers:

- `r0`: pixel x
- `r1`: pixel y
- `r2`: time in seconds
- `r3`: render width
- `r4`: render height
- `r5`: mouse x, reserved
- `r6`: mouse y, reserved
- `r7`: mouse down, reserved

Scratch registers start at `r8`.

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
ret
```

`out` does not stop execution. Use `ret` if the program should end immediately
after writing a color.

