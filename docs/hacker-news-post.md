# Hacker News Draft

## Title

Show HN: ASM Shader Toy, Shadertoy but with a tiny assembly language

## Short Link Text

I made this because a friend was learning assembly and the normal beginner stuff felt boring. Printing numbers, moving bytes around, watching registers change in a terminal. Useful, sure, but not exactly the kind of thing that makes you want to keep poking at it.

So I made a small Shadertoy-like playground where the shader is fake assembly. It runs once per pixel. You get registers, labels, branches, subroutines, includes, constants, textures, webcam/video/audio inputs, and feedback buffers. The default resolution is tiny retro stuff like Game Boy or GBA, then it scales up with nearest-neighbor pixels.

The desktop version started as a CPU VM in C++. That made it easy to understand and debug, but it was obviously not going to be fast forever. The browser version now compiles the assembly to WGSL for WebGPU, with a WebGL2 fallback so Firefox can still run it. That fallback took a little debugging because WGSL switch cases do not fall through and GLSL switch cases do. That one was fun.

It is not meant to be a real ISA. It is a toy machine with enough assembly flavor to teach the useful ideas: registers, control flow, memory-ish inputs, bounded execution, and how tiny instructions add up to an image.

Some fun examples:

- plasma and cellular plasma
- Conway's Game of Life using feedback buffers
- mouse rings
- image and multi-image shaders
- webcam and video effects
- a pixelated planet shader

Browser build:

https://wegfawefgawefg.github.io/asm-shader-toy/

Repo:

https://github.com/wegfawefgawefg/asm-shader-toy

## Possible Comment

I am still sanding this down, but it is already pretty fun to mess with. The language is intentionally fake and small. I wanted something closer to "learn control flow by making pixels move" than "learn assembly by writing another calculator."

The CPU runner is still there because it is useful as the reference implementation. The browser path compiles to GPU shaders because otherwise the fun examples get too slow.

## Notes Before Posting

- Pick a share URL that opens a good default example. `plasma` is safe, `pixelated_planet` is more impressive but heavier.
- Make sure Firefox shows `WebGL2 ready (fallback)` and Chrome shows either `WebGPU ready` or `WebGL2 ready`.
- Be ready to answer "is this real assembly?" with: no, it is a small teaching assembly with registers and branches, designed for pixels.
- Be ready to answer "why not WebAssembly?" with: the point is not to run existing assembly, it is to make a tiny visible machine you can learn by editing.
