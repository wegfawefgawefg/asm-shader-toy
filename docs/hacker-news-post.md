# Hacker News Draft

## Title

Show HN: ASM Shader Toy, Shadertoy but with a tiny assembly language

## Short Link Text

My bud was learning assembly but the normal material was pretty boring but I know he wanted to learn shaders so I combined the two into 
something pretty terrible. It has all the same complexity of normal raymarching, sdf defining, space coordinate warping, except now 
you dont get variables or functions. 

At first it was just a cpp demo with vm that run per pixel. That was slow, but threading it made it plenty fast for whatever peak compute
you might need for a shader written in asm. Then I (gpt) added inputs to match shader toy, buffers for feedback and memory, and various inputs, mouse, kb, mic, webcam, etc. After that i looked over the examples and... found them pretty bland. The asm was missing things needed to make it more of an asm and less of a riscv'ish LHA form. So we add features: registers, labels, branches, subr, includes, consts, etc. 

Theres a clever two eval pass for consts that is inspired by lisp, where the consts are just the same asm but with a different env. I did it 
like that because I looked at real asm const DSL's and they look impure to me and violate the intention of the project. 
Its sort of like comptime if you know what that is and dont know lisp. (What are you retarded?)

I know asm's have lots of funny macro ideas that can make them like pseudo real languages, but I just left that out because at that point 
why not write a little scheme that targets this "ST-ASM" as an IR, and then a tabbed pseudo python with swizzling that lowers to that.
Ill probably do that next for fun.

To share it it had to be on web, and cpu vm per pixel wasnt gonna cut it in js. So the browser version now compiles the assembly to WGSL for WebGPU, with a WebGL2 fallback so Firefox can still run it. (Took longer to get that working than the entire rest of the project, but if it cant run on the top browsers then it can't be shown off...) 

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

## QA

-   Q: It runs like dogshit on my machine"
    A: It probably is running in cpu fallback mode bc ur browser doesnt have some runtime flag on. 

-   Q: "Does this have anything to do with WebAssembly? Why not WebAssembly?"
    A: "Uh its mainly supposed to emit shader code and run on gpu so, didnt really wanna do ffi to wasm for just the cpu fallback. but you could?"

-   Q: "Is this real assembly?"
    A: "Are you made of real meat?"

## Note to self
- Pick a share URL that opens a good default example. `plasma` is safe, `pixelated_planet` is more impressive but heavier.
