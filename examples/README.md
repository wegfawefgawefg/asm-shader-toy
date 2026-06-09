# Examples

Run from the repository root after building:

```sh
./build/asm-shader-toy examples/time_pulse.asm
./build/asm-shader-toy examples/mouse_rings.asm
./build/asm-shader-toy examples/planet_sphere.asm
./build/asm-shader-toy examples/image_passthrough.asm --channel0 examples/assets/checker.png
./build/asm-shader-toy examples/multi_image_mix.asm \
  --channel0 examples/assets/checker.png \
  --channel1 examples/assets/bars.png
```

Validate without opening windows:

```sh
./scripts/validate_examples.sh
```

Multi-file projects use textual `.include` paths relative to the file that
contains the include:

```asm
.include "common/math.inc"
.include <std/screen.inc>
```

`std/screen.inc` also includes `std/aliases.inc`, which defines conventional
scratch names such as `uv_x`, `uv_y`, `pos_x`, `pos_y`, `color_r`, `tex0_r`,
`tex1_r`, and `tmp0`.

The current runner supports static image channels. Webcam, video, audio, and
Shadertoy-style buffer passes are planned but not implemented yet.
