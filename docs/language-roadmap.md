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
```

Likely additions:

```asm
chdim dw, dh, channel
```

`texel` samples pixel coordinates directly. `chdim` would expose channel
dimensions without spending fixed input registers.
