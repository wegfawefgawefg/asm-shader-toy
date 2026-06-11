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
