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

Additional final check at the requested upper test bound:

```text
./build-release/asm-shader-toy examples/perf/heavy.asm --size 800x800 --measure-fps 10
average_fps: 119.219
ms_per_frame: 8.38792
```
