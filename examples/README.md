# Examples

Run from the repository root after building:

```sh
./build/asm-shader-toy examples/time_pulse.asm
./build/asm-shader-toy examples/mouse_rings.asm
./build/asm-shader-toy examples/planet_sphere.asm
./build/asm-shader-toy examples/image_passthrough.asm --channel0 examples/assets/checker.ppm
./build/asm-shader-toy examples/multi_image_mix.asm \
  --channel0 examples/assets/checker.ppm \
  --channel1 examples/assets/bars.ppm
```

Multi-file projects use textual `.include` paths relative to the file that
contains the include:

```asm
.include "common/math.inc"
.include "common/screen.inc"
```

The current runner supports static image channels. Webcam, video, audio, and
Shadertoy-style buffer passes are planned but not implemented yet.

