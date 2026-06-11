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

   Build a small WebGPU page with:

   - code editor
   - compile diagnostics
   - preview canvas
   - size presets
   - channel controls where browser APIs make sense
   - pause/reset

6. **Add share links**

   Encode source and simple settings in the URL hash:

   ```text
   #code=<compressed-base64url>&size=gba&scale=4
   ```

   Use compression such as deflate or LZ-string before base64url encoding. Keep
   multi-file projects out of the first share-link pass unless a compact bundle
   format becomes obvious.

7. **Add richer web channel support**

   In order:

   - generated noise
   - image upload
   - webcam through `getUserMedia`
   - microphone/audio analyser through Web Audio
   - video upload or URL-backed video where browser policy allows

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

## Non-Goals For The First GPU Pass

- Direct Vulkan or hand-written SPIR-V.
- Full Shadertoy compatibility.
- Arbitrary memory writes.
- Dynamic indirect jumps.
- Audio playback synchronization.
- A complete hosted gallery.

## Success Criteria

- A meaningful subset of existing examples runs on GPU.
- GPU and CPU output match closely for deterministic examples.
- The browser can edit, run, pause, reset, and share simple shaders by URL.
- Unsupported programs fail with clear diagnostics instead of undefined output.
