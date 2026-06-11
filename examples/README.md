# Examples

Run from the repository root after building:

```sh
./build/asm-shader-toy examples/basics/consts.asm
./build/asm-shader-toy examples/basics/time_pulse.asm
./build/asm-shader-toy examples/basics/subroutines.asm
./build/asm-shader-toy examples/input/mouse_rings.asm
./build/asm-shader-toy examples/multifile/main.asm
./build/asm-shader-toy examples/raymarch/planet_sphere.asm
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm
./build/asm-shader-toy examples/textures/image_passthrough.asm --channel0 examples/assets/checker.png
./build/asm-shader-toy examples/textures/multi_image_mix.asm \
  --channel0 examples/assets/checker.png \
  --channel1 examples/assets/bars.png
./build/asm-shader-toy examples/buffers/life_display.asm \
  --buffer0 examples/buffers/life_buffer.asm \
  --size gba \
  --scale 4
./build/asm-shader-toy examples/video/video_channel.asm \
  --video0 examples/assets/video/big_buck_bunny_4m34s_640x360.mp4 \
  --size 320x180 \
  --scale 2
./build/asm-shader-toy examples/video/poster_edges.asm \
  --video0 examples/assets/video/big_buck_bunny_4m34s_640x360.mp4 \
  --size 320x180 \
  --scale 2
./build/asm-shader-toy examples/video/channel_metadata.asm \
  --video0 examples/assets/video/big_buck_bunny_4m34s_640x360.mp4 \
  --size 320x180 \
  --scale 2
./build/asm-shader-toy examples/webcam/webcam_channel.asm \
  --webcam0 \
  --size 320x240 \
  --scale 2
./build/asm-shader-toy examples/video/poster_edges.asm \
  --webcam0 \
  --size 320x240 \
  --scale 2
./build/asm-shader-toy examples/audio/audio_scope.asm \
  --audio0 examples/assets/audio/two_tone.wav \
  --size 320x180 \
  --scale 2
./build/asm-shader-toy examples/microphone/mic_scope.asm \
  --mic0 \
  --size 320x180 \
  --scale 2
./build/asm-shader-toy examples/textures/noise_field.asm \
  --noise0 42 \
  --size gba \
  --scale 4
```

Windowed examples hot reload the main file and any `.include` files on save.
Headless `--dry-run`, `--no-graphics`, and `--save-frame` runs do not watch.
Pass `--fps` to show a small window overlay, or `--measure-fps N` to render
`N` CPU frames and print average throughput.
`--size` accepts `WxH` values plus retro presets such as `gb`, `gba`, `nes`,
`snes`, `genesis`, `n64`, `ps1`, `ds`, and `psp`.

Folders:

- `basics/`: time and simple procedural color.
- `input/`: mouse-driven examples.
- `textures/`: static image and generated-noise channel examples.
- `buffers/`: feedback-buffer examples.
- `audio/`: audio-file channel examples.
- `microphone/`: live microphone channel examples.
- `multifile/`: local include project examples.
- `perf/`: intentionally heavy benchmark shaders.
- `raymarch/`: sphere/planet-style visuals.
- `common/`: local includes for examples.
- `assets/`: image assets plus generated and Big Buck Bunny video fixtures.
- `video/`: video-channel example and notes.
- `webcam/`: webcam-channel examples.

Validate without opening windows:

```sh
./scripts/validate_examples.sh
```

Export a still frame:

```sh
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm \
  --size gba \
  --frames 90 \
  --save-frame /tmp/pixel_planet.png
```

Multi-file projects use `.include` paths relative to the file that contains the
include. Includes are once-by-default after canonical path resolution:

```asm
.include "palette.inc"
.include "rings.inc"
.include <std/screen.inc>
```

`std/screen.inc` also includes `std/aliases.inc`, which defines conventional
scratch names such as `uv_x`, `uv_y`, `pos_x`, `pos_y`, `color_r`, `tex0_r`,
`tex1_r`, and `tmp0`. Standard aliases are reserved, so user `.alias`
directives cannot redefine them.

The current runner supports static image channels, streaming video channels
through local `ffmpeg`/`ffprobe`, webcam channels through local `ffmpeg`/V4L2,
audio channels, microphone channels through local `ffmpeg`/PulseAudio,
generated noise channels, and four feedback buffer passes.

`raymarch/pixelated_planet.asm` is adapted from the CC0 Godot Shaders post
"3D Pixelated Planet", which credits the YouTube short as inspiration.
