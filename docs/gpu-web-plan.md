# GPU And Web Plan

The next major direction is a GPU/Web backend that keeps the CPU VM as the
reference implementation. The first target should be WGSL/WebGPU, not raw
SPIR-V/Vulkan.

## Direction

Use this shape:

```text
asm source -> assembler -> validated IR -> CPU VM
                                |
                                -> WGSL compute shader -> WebGPU/native GPU
```

The CPU renderer remains the compatibility and debugging path. The GPU renderer
is an accelerating backend for programs that fit the validated subset.

## Current Implementation Status

- A lowered IR boundary exists in `include/ast/ir.hpp` and `src/ir.cpp`.
- The IR records source locations, operands, feature flags, and direct
  successors for labels and branches.
- The CPU runtime now lowers `Program` to `IrProgram` before execution, while the
  opcode semantics remain shared through the same VM executor.
- Frame rendering lowers once per frame, not once per pixel.
- Core tests cover IR feature classification, invalid jump-target diagnostics,
  and CPU parity between assembled programs and lowered IR.
- Web tests cover selected native C++ `--emit-wgsl` vs browser TypeScript WGSL
  compiler parity fixtures when `AST_NATIVE_CLI` points at the built native CLI.
- Initial WGSL emission exists in `include/ast/wgsl.hpp` and `src/wgsl.cpp`.
  It currently targets deterministic arithmetic, unary math, comparisons,
  direct branches, bounded program-counter loops, `tex`, `texel`, `chdim`,
  `chtime`, `chsrate`, `key`, `mbtn`, `mwheel`, `gbtn`, `gaxis`, `out`,
  `out8`, `call`, `ret`, and `halt`.
- The native CLI exposes that compiler through `--emit-wgsl path|-`, which
  writes WGSL and exits before SDL/media setup.
- The native CLI also exposes `--emit-wgsl-bundle dir`, which writes
  `image.wgsl`, any `bufferN.wgsl` feedback pass shaders, and a small manifest
  without initializing SDL/media.
- An optional `AST_BUILD_NATIVE_WEBGPU=ON` CMake path now fetches a pinned
  WebGPU-distribution/wgpu-native backend and builds `ast-webgpu-probe`, which
  requests a native adapter/device, dispatches a small handwritten compute
  shader, runs WGSL emitted from asm into an `rgba8unorm` storage texture, reads
  data back to CPU memory, and verifies native WebGPU execution works on
  supported hosts. The optional build also creates `ast-webgpu-frame`, an
  experimental headless renderer that runs one deterministic asm image pass on
  native WebGPU, uploads static image and generated noise channels, renders
  feedback buffer passes, samples deterministic video and audio file frames, can
  write a PPM frame, and can compare GPU readback against the CPU VM.
  `scripts/validate_webgpu_frame.sh` runs a small CPU-vs-GPU parity suite for
  supported deterministic examples, including one-image, two-image, and noise
  texture programs plus deterministic feedback-buffer, video texel, and audio
  texture programs.
- `ast-webgpu-surface-probe` opens an SDL window on Linux/X11, creates a native
  WebGPU surface, clears it, and presents frames. This proves the surface path
  independently from shader execution.
- `ast-webgpu-run` opens an SDL window on Linux/X11, runs emitted asm WGSL on
  native WebGPU, and presents the intermediate texture with nearest-neighbor
  integer scaling. It supports the same static image, generated noise,
  deterministic video/audio file, and feedback-buffer inputs as
  `ast-webgpu-frame`, plus live mirrored webcam and microphone channels uploaded
  each frame.
- The WGSL emitter returns diagnostics for unsupported ops instead of silently
  falling back.
- A browser app scaffold exists under `web/`. It can edit a multi-file project,
  import/export JSON bundles, copy compressed share URLs, edit WGSL, and run
  that WGSL through WebGPU into a nearest-neighbor canvas. Browser-side asm
  compilation exists for a prototype subset covering includes, aliases,
  `.const`, `.consts`, labels, branches, calls, arithmetic, texture/channel
  metadata ops, live input query ops, and output. Browser channel controls can
  bind static images, generated noise textures, or mirrored webcam streams to
  `channel0..3`, upload live microphone analyser data or user-selected audio
  files as 512x2 audio textures, and use user-selected video files as looping
  channel textures. URL-backed video channels are also supported when the remote
  server permits browser media/CORS access. Browser feedback buffers can be
  assigned from project files and are rendered with ping-pong textures before
  the final image pass. The browser preview supports pause, reset, FPS display,
  and PNG frame export.
  ASM and WGSL edits hot-compile after a short debounce.
  Image/noise channels are preserved in project bundles; webcam, microphone,
  audio, and video channels preserve metadata and reconnect through browser
  permission or file selection.

## Why WGSL First

- Browsers expose WebGPU/WGSL, not Vulkan/SPIR-V.
- WGSL can be shared by a native WebGPU runner and a browser runner.
- WebGPU avoids most direct Vulkan setup work: device selection, descriptor
  details, pipeline portability, and validation-layer churn.
- A SPIR-V/Vulkan backend can remain optional later if native-only performance
  or integration requires it.

## Milestones

1. **Define a lowered IR**

   Add an explicit validated representation after assembly and before execution.
   The IR should resolve labels, validate operands, classify runtime-only ops,
   and expose control flow in a form both CPU and GPU backends can consume.

2. **Preserve CPU as the reference backend**

   Keep existing VM behavior and tests. Add CPU-vs-GPU image comparisons for a
   small suite once the GPU path exists. The CPU path should continue to support
   diagnostics, low-level debugging, and headless validation.

3. **Emit WGSL for a small subset**

   Start with:

   - arithmetic and unary math
   - `mov`, `min`, `max`, `mod`, comparisons
   - direct branches and bounded loops
   - `out` and `out8`
   - `tex`, `texel`, `chdim`, `chtime`, `chsrate`
   - fixed input registers and live scalar inputs

   Defer complicated cases only if necessary, but do not silently fall back per
   instruction. A program should either compile for GPU or report why it cannot.

4. **Implement native WebGPU runner**

   Prefer `wgpu-native` or Dawn over direct Vulkan. The runner should render the
   same intermediate texture size and present it with nearest-neighbor scaling.
   Keep the current SDL CPU runner intact while the GPU runner stabilizes. A
   first SDL/X11 runner now exists as `ast-webgpu-run`.

5. **Implement browser runner**

   Build a WebGPU page with the same authoring model as the native runner:

   - code editor
   - in-browser project file tree
   - selected main file
   - `.include` resolution across project files and bundled stdlib files
   - compile diagnostics
   - preview canvas
   - size presets
   - channel controls
   - pause/reset
   - FPS display
   - save/export frame
   - import/export project
   - share links

6. **Add share links**

   Encode project files and simple settings in the URL hash:

   ```text
   #project=<compressed-base64url>&main=main.asm&size=gba&scale=4
   ```

   Use compression such as deflate or LZ-string before base64url encoding. Keep
   the bundle format simple: file path plus text content plus selected settings.

7. **Add richer web channel support**

   In order:

   - generated noise. Done in the browser prototype.
   - image upload. Done in the browser prototype.
   - webcam through `getUserMedia`. Done in the browser prototype.
   - microphone/audio analyser through Web Audio. Done in the browser prototype.
   - video upload or URL-backed video where browser policy allows. Done in the
     browser prototype.

## Website Parity Target

The website should aim for the native runner's utility surface, not a separate
mini-language or a reduced demo. It should preserve the same project model,
standard include behavior, shader inputs, channel types, feedback buffers,
runtime controls, and sharing/import/export utilities wherever browser APIs make
that possible.

Language features to support:

- `.include`, including local project files and bundled `stdlib/std` files
- `.alias`, `.const`, and `.consts`
- global and local labels
- branches, `call`, `ret`, and `halt`
- `tex`, `texel`, `chdim`, `chtime`, and `chsrate`
- `key`, `mbtn`, `mwheel`, `gbtn`, and `gaxis`
- feedback buffer passes. Done in the browser prototype.

Runtime/editor features to support:

- multi-file project tree with a selected entrypoint
- hot compile on edit. Done in the browser prototype.
- diagnostics with source file and line information
- pause, reset, FPS, and size presets
- image upload/drop for channels
- generated noise channels
- webcam through `getUserMedia`
- microphone and audio analyser through Web Audio
- video through upload or user-selected media
- save frame
- import/export project bundle
- compressed share URL

Useful libraries are acceptable when they reduce risk:

- CodeMirror 6 or Monaco for editing, with CodeMirror preferred for weight and
  custom language highlighting.
- Vite for a small browser app build.
- `fflate` or `lz-string` for compressed share links.
- Browser WebGPU APIs directly, with a small helper library only if it removes
  real boilerplate.
- Browser-native media APIs for image, video, webcam, microphone, and audio.

The assembler/compiler behavior must not drift from native. Either share the
compiler implementation, compile the C++ assembler to WASM, or keep a
TypeScript implementation covered by the same golden tests as the C++ assembler.
For velocity, the browser currently uses a TypeScript compiler prototype with
native-vs-browser WGSL parity fixtures in `web/src/nativeParity.test.ts`.

## Hard Parts

- Control flow lowering: labels and branches need to map to structured WGSL or a
  simulated program-counter loop.
- Calls: subroutines may need inlining, a bounded call stack, or a validated
  subset before WGSL can support them cleanly.
- Feedback buffers: WebGPU storage/texture ping-pong must match CPU buffer
  semantics.
- Texture semantics: current `tex` is nearest and clamp-to-edge; GPU sampling
  must match that exactly.
- Diagnostics: GPU compile errors should point back to asm source lines where
  possible.
- Web media APIs: webcam, microphone, and audio all have browser permission and
  autoplay constraints.
- Compiler parity: a web assembler/compiler can diverge from native behavior if
  it is not shared or covered by golden tests.

## Non-Goals For The First GPU Pass

- Direct Vulkan or hand-written SPIR-V.
- Full Shadertoy compatibility.
- Arbitrary memory writes.
- Dynamic indirect jumps.
- Audio playback synchronization.
- A complete hosted gallery.
- A permanent second language dialect for the browser.

## Success Criteria

- A meaningful subset of existing examples runs on GPU.
- GPU and CPU output match closely for deterministic examples.
- The browser can edit multi-file projects, run, pause, reset, and share shaders
  by URL.
- Unsupported programs fail with clear diagnostics instead of undefined output.
