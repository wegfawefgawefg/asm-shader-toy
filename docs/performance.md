# Performance Notes

Benchmarks use release builds and `--measure-fps`, which times CPU rendering
without window upload or present. Test resolutions stay at or below `800x800`.

## Heavy Shader

`examples/perf/heavy.asm` is the benchmark fixture. It intentionally mixes
arithmetic, `sin`, `cos`, `sqrt`, `floor`, `mod`, comparisons, branches, and
multiple output paths so runtime changes are tested against a non-trivial VM
program.

## Comparison Matrix

| ID | Proposed optimization | Status | Command | Avg FPS | ms/frame | Speedup | Decision | Notes |
| --- | --- | --- | --- | ---: | ---: | ---: | --- | --- |
| B0 | Baseline release runtime | Measured | `./build-release/asm-shader-toy examples/perf/heavy.asm --size 512x512 --measure-fps 30` | 26.74 | 37.40 | 1.00x | Baseline | Mean of 3 runs: 26.39, 26.96, 26.87 FPS. |
| O1 | Reuse frame-level input/register setup in `render_frame` | Measured | Same as B0 | 27.62 | 36.22 | 1.03x | Accepted | Mean of 3 runs: 27.70, 27.03, 28.12 FPS. Avoids per-pixel `PixelInputs` construction and repeated invariant register stores. |
| O2 | Store instruction operands inline | Measured | Same as B0 | 28.96 | 34.54 | 1.08x | Accepted | Mean of 3 runs: 28.90, 28.87, 29.10 FPS. Replaces per-instruction heap-backed operand vectors with fixed operand storage. |
| O3 | Cache VM code pointer and size in pixel loop | Measured | Same as B0 | 28.68 | 34.88 | 1.07x | Rejected | Mean of 3 runs: 28.67, 28.39, 28.96 FPS. Slower than accepted O2 state, likely noise/codegen did not improve; change was removed. |
| O4 | Seed row registers and write through row pointer | Measured | Same as B0 | 28.61 | 34.95 | 1.07x | Rejected | Mean of 3 runs: 28.68, 28.61, 28.55 FPS. Slower than accepted O2 state; change was removed. |
| O5 | Parallelize `render_frame` by row bands | Measured | Same as B0 | 258.65 | 3.89 | 9.67x | Accepted | Final mean of 3 runs: 252.97, 287.28, 235.70 FPS on a 32-thread machine. Large win, but noisy because workers are recreated each frame. |
| O6 | Cap render worker count at 8 | Measured | Same as B0 | 154.48 | 6.89 | 5.78x | Rejected | Mean of 3 runs: 212.79, 125.82, 124.84 FPS. Slower than O5, so the cap was removed. |
| O7 | Lower parallel threshold for retro resolutions | Measured | `./build-release/asm-shader-toy examples/raymarch/pixelated_planet.asm --size gba --measure-fps 60` | 1028.48 | 0.97 | 8.61x vs GBA baseline | Accepted | Mean of 3 runs: 976.89, 1060.12, 1048.43 FPS. Fixes the GBA-vs-PSP anomaly by letting 38,400-pixel GBA frames use parallel rendering. Follow-up threshold sweep set the final cutoff to 4,096 pixels. |
| O8 | Persistent render worker pool | Measured | `./build-release/asm-shader-toy examples/perf/heavy.asm --size 512x512 --measure-fps 60` | 300.49 | 3.33 | 0.98x vs O7 state | Rejected | Mean of 3 runs: 291.21, 294.40, 315.87 FPS. Fresh pre-change baseline was 306.29 FPS. It helped `pixelated_planet.asm` at GBA slightly, 1095.74 vs 1076.98 FPS, but regressed heavy 512 and PSP, 369.11 vs 398.84 FPS, so the pool was removed. |

Additional final check at the requested upper test bound:

```text
./build-release/asm-shader-toy examples/perf/heavy.asm --size 800x800 --measure-fps 10
average_fps: 119.219
ms_per_frame: 8.38792
```

## Next Optimization Candidates

These are not accepted until measured against the benchmark matrix above.

- Persistent worker team v2: retry worker pooling only with cheaper per-frame
  dispatch, fixed per-worker row ranges, padded per-worker state, and a measured
  fallback. O8's condition-variable pool regressed, so this should stay behind a
  benchmark gate.
- Predecoded fast instruction format: lower each instruction into fixed source
  register/immediate fields so the VM does less `OperandKind` branching in the
  inner loop.
- Specialized hot op execution: flatten common operations like `mov`, `add`,
  `mul`, `gt`, `jnz`, and `out` to avoid generic operand lambdas on the hottest
  paths.
- SIMD pixel batches: run multiple pixels together for branch-coherent shaders.
  This is harder because VM branches can diverge.
- More representative benchmark mode: benchmark startup/warmup should separate
  steady-state frame cost from setup cost.
- Streaming video channels: `--videoN` currently preloads all decoded frames,
  which is fine for short fixtures but inappropriate for long movies. Decode a
  small ring of frames on demand before encouraging arbitrary user videos.
- Optimized IR pass: constant folding, dead path cleanup, and tighter bytecode
  packing before interpretation.
- GPU backend later: compile the asm IR to WGSL/GLSL after the CPU VM/debugger
  model is solid.

## Open Investigation

- `pixelated_planet.asm` appears to run worse at `--size gba` than at
  `--size psp`. Hypothesis: `gba` is `240x160` (38,400 pixels), below the
  current 65,536-pixel parallel-render threshold, while `psp` is `480x272`
  (130,560 pixels) and renders across all workers.

Baseline measurements before O7:

| Shader | Size | Pixels | Avg FPS | ms/frame | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `pixelated_planet.asm` | `gba` (`240x160`) | 38,400 | 119.41 | 8.38 | Single-threaded due to threshold. Mean of 119.80, 118.19, 120.23 FPS. |
| `pixelated_planet.asm` | `psp` (`480x272`) | 130,560 | 387.50 | 2.59 | Parallel path. Mean of 378.54, 416.04, 367.92 FPS. |

O7 lowered the parallel-render threshold from 65,536 pixels. A follow-up sweep
showed that very tiny frames should stay single-threaded, while `64x64` and
larger should use workers. The final cutoff is 4,096 pixels.
After O7:

| Shader | Size | Pixels | Avg FPS | ms/frame | Notes |
| --- | --- | ---: | ---: | ---: | --- |
| `pixelated_planet.asm` | `gba` (`240x160`) | 38,400 | 1028.48 | 0.97 | Parallel path. Mean of 976.89, 1060.12, 1048.43 FPS. |
| `pixelated_planet.asm` | `psp` (`480x272`) | 130,560 | 392.35 | 2.55 | Still parallel, roughly unchanged. Mean of 389.88, 391.17, 396.02 FPS. |
| `pixelated_planet.asm` | `64x64` | 4,096 | 2301.76 | 0.43 | Parallel path is faster than single-threaded 1104.43 FPS at this size. |

Threshold sweep:

| Shader | Size | Pixels | Single FPS | Parallel FPS | Winner |
| --- | --- | ---: | ---: | ---: | --- |
| `pixelated_planet.asm` | `32x32` | 1,024 | 4391.06 | 2472.86 | Single |
| `pixelated_planet.asm` | `40x40` | 1,600 | 2787.75 | 2195.00 | Single |
| `pixelated_planet.asm` | `48x48` | 2,304 | 1942.58 | 2055.02 | Parallel, small margin |
| `pixelated_planet.asm` | `56x56` | 3,136 | 1430.66 | 2018.89 | Parallel |
| `pixelated_planet.asm` | `64x64` | 4,096 | 1104.43 | 2301.76 | Parallel |
| `heavy.asm` | `32x32` | 1,024 | 6498.29 | 2478.90 | Single |
| `heavy.asm` | `40x40` | 1,600 | 4234.20 | 2133.99 | Single |
| `heavy.asm` | `48x48` | 2,304 | 2854.42 | 1980.43 | Single |
| `heavy.asm` | `56x56` | 3,136 | 2183.97 | 2082.68 | Single, small margin |
| `heavy.asm` | `64x64` | 4,096 | 1655.02 | 2261.36 | Parallel |

Decision: keep frames below `64x64` single-threaded. Use the parallel path at
`64x64` and above, which includes all retro presets currently supported by
`--size`.
