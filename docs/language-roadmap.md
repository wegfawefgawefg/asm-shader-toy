# Language Roadmap

The assembly should stay small, but it needs enough structure to make visual
programs pleasant.

## Multi-File

Current support is path-based include:

```asm
.include "palette.asm"
.include "noise.asm"
```

Each include is once-by-default after canonical path resolution, so common files
can be included through multiple paths without duplicate labels. Standard-library
aliases and built-in input aliases are reserved and cannot be redefined by user
`.alias` directives.

Runnable examples live in `examples/`:

- `examples/common/math.inc`
- `stdlib/std/aliases.inc`
- `stdlib/std/screen.inc`
- `examples/basics/time_pulse.asm`
- `examples/input/mouse_rings.asm`
- `examples/buffers/life_display.asm`
- `examples/buffers/life_buffer.asm`
- `examples/multifile/main.asm`
- `examples/textures/image_passthrough.asm`
- `examples/textures/multi_image_mix.asm`
- `examples/raymarch/planet_sphere.asm`
- `examples/raymarch/pixelated_planet.asm`
- `examples/video/video_channel.asm`
- `examples/webcam/webcam_channel.asm`
- `examples/assets/video/testsrc_160x90.mp4`

## Subroutines

Reusable helper blocks are supported with `call`, `ret`, and `halt`:

```asm
call palette
halt

palette:
    ; inputs and outputs are agreed registers
    ret
```

`call` pushes a return address onto a bounded VM call stack. `ret` returns from
`call`, or halts if the stack is empty so old programs keep working. `halt`
always exits the current pixel program.

## Branches And Local Labels

Branch targets can use global labels or local labels scoped under the previous
global label:

```asm
palette:
    jlt tmp0, 2.0, .blue
    jmp .pink
.blue:
    ret
.pink:
    ret
```

Current branch instructions are `jmp`, `jnz`, `jz`, `jeq`, `jne`, `jlt`, `jle`,
`jgt`, and `jge`.

## Buffers

The first buffer implementation treats buffers as extra feedback render passes:

```text
asm-shader-toy image.asm --buffer0 feedback.asm --buffer1 blur.asm
```

Buffer N renders into channel N. Buffer passes see previous-frame buffer
contents, then the final image pass sees the newly rendered buffers. This is
enough for stateful effects such as cellular automata:

```sh
./build/asm-shader-toy examples/buffers/life_display.asm \
  --buffer0 examples/buffers/life_buffer.asm
```

Current frame order:

1. Expose previous-frame buffer textures as channels.
2. Render buffer passes into intermediate textures.
3. Expose current buffer textures as channels.
4. Render the final image pass.

Useful next buffer features:

- explicit buffer-to-channel binding instead of fixed buffer N to channel N
- per-buffer max step limits
- current-frame dependencies between buffer passes when wanted

## Channel Instructions

Current:

```asm
tex dr, dg, db, da, channel, u, v
texel dr, dg, db, da, channel, x, y
chdim dw, dh, channel
chtime dt, channel
```

`texel` samples pixel coordinates directly. `chdim` exposes channel dimensions
without spending fixed input registers. `chtime` exposes channel-local time in
seconds.
