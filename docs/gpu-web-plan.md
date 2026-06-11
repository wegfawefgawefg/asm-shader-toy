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
- Initial WGSL emission exists in `include/ast/wgsl.hpp` and `src/wgsl.cpp`.
  It currently targets deterministic arithmetic, unary math, comparisons,
  direct branches, bounded program-counter loops, `tex`, `texel`, `chdim`,
  `chtime`, `chsrate`, `key`, `mbtn`, `mwheel`, `gbtn`, `gaxis`, `out`,
  `out8`, `call`, `ret`, and `halt`.
- The native CLI exposes that compiler through `--emit-wgsl path|-`, which
  writes WGSL and exits before SDL/media setup.
- The WGSL emitter returns diagnostics for unsupported ops instead of silently
  falling back. Native WebGPU execution and the browser runner are still
  unimplemented.
- A browser app scaffold exists under `web/`. It can edit a multi-file project,
  import/export JSON bundles, copy compressed share URLs, edit WGSL, and run
  that WGSL through WebGPU into a nearest-neighbor canvas. Browser-side asm
  compilation exists for a prototype subset covering includes, aliases,
  `.const`, `.consts`, labels, branches, calls, arithmetic, texture/channel
  metadata ops, live input query ops, and output. Browser image upload controls
  can bind static images to `channel0..3` and preserve them in project bundles.
  Generated noise, webcam, microphone/audio, video, feedback buffers, and native
  WebGPU execution are still unimplemented.

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
   Keep the current SDL CPU runner intact while the GPU runner stabilizes.

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

   - generated noise
   - image upload
   - webcam through `getUserMedia`
   - microphone/audio analyser through Web Audio
   - video upload or URL-backed video where browser policy allows

## Website Parity Target

The website should aim for the native runner's utility surface, not a separate
mini-language.

Language features to support:

- `.include`, including local project files and bundled `stdlib/std` files
- `.alias`, `.const`, and `.consts`
- global and local labels
- branches, `call`, `ret`, and `halt`
- `tex`, `texel`, `chdim`, `chtime`, and `chsrate`
- `key`, `mbtn`, `mwheel`, `gbtn`, and `gaxis`
- feedback buffer passes

Runtime/editor features to support:

- multi-file project tree with a selected entrypoint
- hot compile on edit
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
For velocity, a TypeScript compiler prototype is acceptable only if parity tests
are added early.

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
