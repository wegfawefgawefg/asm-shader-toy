# Native WebGPU Bootstrap

The native WebGPU path is optional and default-off while the renderer is under
development:

```sh
./scripts/build_webgpu_probe.sh
./build-webgpu-probe/ast-webgpu-probe
```

This configures CMake with `AST_BUILD_NATIVE_WEBGPU=ON`, fetches a pinned
`eliemichel/WebGPU-distribution` tag, links the `webgpu` CMake target, and copies
the backend shared library next to the probe executable. The probe requests an
adapter/device, dispatches a small handwritten WGSL compute shader, then
assembles asm source, emits WGSL through the native compiler, runs that shader
into an `rgba8unorm` storage texture, and verifies CPU readback pixels.

The same optional build also creates `ast-webgpu-frame`, an experimental
headless renderer for deterministic single-image programs:

```sh
./build-webgpu-probe/ast-webgpu-frame examples/basics/plasma.asm \
  --size gba \
  --compare-cpu \
  --output /tmp/asm-shader-toy-gpu.ppm

./build-webgpu-probe/ast-webgpu-frame examples/textures/multi_image_mix.asm \
  --size 64x64 \
  --channel0 examples/assets/checker.png \
  --channel1 examples/assets/bars.png \
  --compare-cpu

./build-webgpu-probe/ast-webgpu-frame examples/textures/noise_field.asm \
  --size 64x64 \
  --noise0 42 \
  --compare-cpu

./build-webgpu-probe/ast-webgpu-frame examples/buffers/ramp_display.asm \
  --buffer0 examples/buffers/ramp_buffer.asm \
  --size 32x24 \
  --frames 4 \
  --compare-cpu

./build-webgpu-probe/ast-webgpu-frame examples/video/video_texel.asm \
  --video0 examples/assets/video/testsrc_160x90.mp4 \
  --size 160x90 \
  --time 0.5 \
  --compare-cpu

./build-webgpu-probe/ast-webgpu-frame examples/audio/audio_scope.asm \
  --audio0 examples/assets/audio/two_tone.wav \
  --size 64x32 \
  --time 0.25 \
  --compare-cpu

./build-webgpu-probe/ast-webgpu-run examples/basics/plasma.asm \
  --size gba \
  --scale 4

./build-webgpu-probe/ast-webgpu-run examples/webcam/webcam_channel.asm \
  --webcam0 \
  --size gba \
  --scale 4

./build-webgpu-probe/ast-webgpu-run examples/microphone/mic_scope.asm \
  --mic0 \
  --size gba \
  --scale 4

./build-webgpu-probe/ast-webgpu-surface-probe --size 160x90 --frames 60 --scale 2
```

It dispatches emitted asm WGSL, copies the storage texture back to CPU memory,
optionally compares the result with the CPU VM, and writes a binary PPM frame
when `--output` is provided. Static image channels, generated noise channels,
and feedback buffer passes are uploaded or rendered as `rgba8unorm` textures and
use the same metadata uniforms as the CPU VM. Video file channels decode one
deterministic frame at the requested shader time and upload it as a channel
texture. Audio file channels decode through ffmpeg and upload the same 512x2
waveform/spectrum texture shape as the CPU runner.

`ast-webgpu-run` is the first visible native WebGPU runner. It opens an SDL
window on Linux/X11, runs emitted asm WGSL into the same intermediate
`rgba8unorm` texture used by the headless frame tool, and presents that texture
to the window with nearest-neighbor integer scaling. It currently supports the
same static image, generated noise, deterministic video/audio file, and feedback
buffer inputs as `ast-webgpu-frame`. It also supports live mirrored webcam
channels by streaming frames through ffmpeg and uploading the latest complete
frame into the GPU channel texture before dispatch. Live microphone channels use
the same ffmpeg/PulseAudio capture path and `512x2` waveform/spectrum texture
shape as the CPU runner.

`ast-webgpu-surface-probe` is a narrower presentation smoke test. On Linux/X11,
it opens an SDL window, creates a WebGPU surface from the native window handle,
configures the surface, clears it for a few frames, and presents. It does not
run emitted asm WGSL.

## Main Runner Integration Plan

The native GPU path should eventually move from separate experimental tools
into the normal `asm-shader-toy` executable behind an explicit flag:

```sh
./build/asm-shader-toy examples/basics/plasma.asm --gpu --size gba --scale 4
```

Target behavior:

- Keep CPU rendering as the default until the GPU path has matching coverage.
- Add `--gpu` to use the existing native WebGPU window runner pipeline.
- Reuse the same assembler, WGSL compiler, input metadata, feedback buffer
  ping-pong, and nearest-neighbor presentation rules as the web runner.
- Support the same practical channel set as the CPU runner: images, generated
  noise, video files, audio files, webcam, microphone, and feedback buffers.
- Preserve existing hot reload behavior for the main program and includes.
- Report GPU compilation/device errors clearly and keep the last good program
  alive when a reload fails.
- Add `--gpu --measure-fps N` or an equivalent benchmark path once interactive
  rendering is stable.

Implementation notes:

- `ast-webgpu-run` is the best source to merge from: it already opens a window,
  dispatches emitted ASM WGSL into an intermediate texture, and presents it with
  nearest-neighbor scaling.
- `ast-webgpu-frame` remains useful as the deterministic parity harness. Keep it
  as a CI/debug tool even after `--gpu` lands in the main executable.
- The main risk is avoiding two parallel renderers that drift. Shared helpers
  for channel upload, buffer passes, uniform packing, and presentation should be
  extracted before or during the `--gpu` integration.

Run the repeatable native GPU parity gate:

```sh
./scripts/validate_webgpu_frame.sh
```

The current default uses:

- `AST_WEBGPU_DISTRIBUTION_TAG=main-v0.2.0`
- `AST_WEBGPU_BACKEND=WGPU`
- `WEBGPU_BUILD_FROM_SOURCE=OFF`
- `WEBGPU_LINK_TYPE=SHARED`

The choice follows WebGPU-distribution's documented CMake integration: it
provides a `webgpu` target, supports backend selection, and advises using an
explicit tag or commit rather than a moving branch. The upstream wgpu-native
project documents prebuilt Linux/macOS/Windows binaries, which keeps this probe
cheap enough to build without vendoring a large renderer dependency into the
repository.

References:

- https://github.com/eliemichel/WebGPU-distribution
- https://github.com/gfx-rs/wgpu-native
