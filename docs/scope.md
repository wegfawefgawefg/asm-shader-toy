# Scope

This project starts as a CPU-rendered toy, not a fast shader compiler.

## Version 0 Shape

- C++20.
- SDL2 window.
- Default intermediate resolution: `240x160`.
- Default window scale: `4x`.
- One scalar VM invocation per pixel.
- One uploaded streaming texture per frame.
- No interpreter work during texture-to-window scaling.
- Simple assembler with labels, constants, comments, and includes.

## Performance Stance

The scalar VM is expected to be slow at modern resolutions. That is acceptable
for the first version because the useful learning surface is:

- editing a tiny assembly program
- running it across a framebuffer
- inspecting the language and VM behavior
- later adding stepping for one chosen pixel

The first optimization targets should be:

- frame tiling and worker threads
- bytecode validation to simplify runtime checks
- SIMD batches where one VM register stores multiple pixel lanes
- optional native or GPU backend only after the language settles

## Non-Goals For Now

- Full Shadertoy compatibility.
- Arbitrary memory writes.
- Dynamic indirect jumps.
- Unbounded programs.
- GPU shader generation.
- A polished editor UI.

## Open Design Questions

- Whether color output should stay normalized-first or prefer byte output.
- Whether loops should use only `jmp`/`jnz` or gain counted-loop helpers.
- Whether multi-file programs should remain textual `.include` or become a
  module system.
- How much debugging UI belongs in the SDL runner versus separate CLI tools.

