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
```

It dispatches emitted asm WGSL, copies the storage texture back to CPU memory,
optionally compares the result with the CPU VM, and writes a binary PPM frame
when `--output` is provided. SDL presentation, loaded media channels, and
feedback buffers are still future native WebGPU work.

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
