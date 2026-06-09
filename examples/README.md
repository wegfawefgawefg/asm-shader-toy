# Examples

Run from the repository root after building:

```sh
./build/asm-shader-toy examples/basics/time_pulse.asm
./build/asm-shader-toy examples/input/mouse_rings.asm
./build/asm-shader-toy examples/multifile/main.asm
./build/asm-shader-toy examples/raymarch/planet_sphere.asm
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm
./build/asm-shader-toy examples/textures/image_passthrough.asm --channel0 examples/assets/checker.png
./build/asm-shader-toy examples/textures/multi_image_mix.asm \
  --channel0 examples/assets/checker.png \
  --channel1 examples/assets/bars.png
```

Folders:

- `basics/`: time and simple procedural color.
- `input/`: mouse-driven examples.
- `textures/`: static image channel examples.
- `multifile/`: local include project examples.
- `raymarch/`: sphere/planet-style visuals.
- `common/`: local includes for examples.
- `assets/`: image assets, plus a future video fixture.
- `video/`: future video-channel sketch and notes.

Validate without opening windows:

```sh
./scripts/validate_examples.sh
```

Export a still frame:

```sh
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm \
  --size 506x632 \
  --frames 90 \
  --save-frame /tmp/pixel_planet.png
```

Multi-file projects use textual `.include` paths relative to the file that
contains the include:

```asm
.include "palette.inc"
.include "rings.inc"
.include <std/screen.inc>
```

`std/screen.inc` also includes `std/aliases.inc`, which defines conventional
scratch names such as `uv_x`, `uv_y`, `pos_x`, `pos_y`, `color_r`, `tex0_r`,
`tex1_r`, and `tmp0`.

The current runner supports static image channels. Webcam, video, audio, and
Shadertoy-style buffer passes are planned but not implemented yet.
See `examples/video/README.md` for the included MP4 fixture and future command
shape.

`raymarch/pixelated_planet.asm` is adapted from the CC0 Godot Shaders post
"3D Pixelated Planet", which credits the YouTube short as inspiration.
