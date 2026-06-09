# Language Roadmap

The assembly should stay small, but it needs enough structure to make visual
programs pleasant.

## Multi-File

Current support is textual include:

```asm
.include "palette.asm"
.include "noise.asm"
```

That is enough for shared constants, aliases, and helper blocks. The next useful
step is include guards or a `.once` directive so common files can be included by
multiple files without duplicate labels.

Runnable examples live in `examples/`:

- `examples/common/math.inc`
- `stdlib/std/aliases.inc`
- `stdlib/std/screen.inc`
- `examples/basics/time_pulse.asm`
- `examples/input/mouse_rings.asm`
- `examples/textures/image_passthrough.asm`
- `examples/textures/multi_image_mix.asm`
- `examples/raymarch/planet_sphere.asm`
- `examples/assets/video/testsrc_160x90.mp4` exists as a future fixture; video
  channels are not implemented yet.

## Subroutines

Labels plus `jmp`/`jnz` are enough for loops, but not for reusable functions.
The likely shape is:

```asm
call palette
halt

palette:
    ; inputs and outputs are agreed registers
    ret
```

That requires a small VM call stack and a separate `halt` instruction. The
current `ret` exits the whole program, so adding subroutines should be done as a
deliberate compatibility break or with a transition:

- add `halt`
- make `call` push return addresses
- make `ret` return from `call`, or halt if the stack is empty
- cap call depth to keep bad programs bounded

## Buffers

Shadertoy-style buffers are not just images; they are extra passes. A reasonable
CPU version should treat them as named render targets:

```text
program.asm
buffers/
  feedback.asm
  blur.asm
```

Potential CLI shape:

```sh
asm-shader-toy image.asm --buffer0 feedback.asm --buffer1 blur.asm
```

The frame order would be:

1. Render buffer passes into intermediate textures.
2. Expose those textures as channels to later passes.
3. Render the final image pass.

Useful first buffer features:

- fixed buffer size defaults to image size
- previous-frame channel for feedback
- explicit buffer-to-channel binding
- per-buffer max step limits

This should wait until image channels feel solid, because buffers can reuse the
same `tex` sampling machinery.

## Channel Instructions

Current:

```asm
tex dr, dg, db, da, channel, u, v
```

Likely additions:

```asm
texel dr, dg, db, da, channel, x, y
chdim dw, dh, channel
```

`texel` samples pixel coordinates directly. `chdim` exposes channel dimensions
without spending fixed input registers.
