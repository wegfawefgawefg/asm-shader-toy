# asm-shader-toy

`asm-shader-toy` is a tiny Shadertoy-like experiment where the shader is a small
assembly language run once per pixel on the CPU.

The first target is deliberately modest: GBA-resolution intermediate rendering
at `240x160`, scaled up into a normal SDL window. The VM is scalar for now so
the language and debugger surface can stay easy to understand before any SIMD,
threaded, or GPU backend exists.

## Build

```sh
./scripts/build.sh
```

## Run

```sh
./scripts/run.sh
```

Or run a specific program:

```sh
./build/asm-shader-toy examples/plasma.asm --size 240x160 --dimscale 4
```

`--scale` and `--dimscale` are aliases. The interpreter always renders the
intermediate texture size; SDL handles scaling that texture into the window.

## Current Model

Every pixel starts with these registers:

- `r0`: pixel x
- `r1`: pixel y
- `r2`: time in seconds
- `r3`: render width
- `r4`: render height
- `r5`: mouse x, reserved
- `r6`: mouse y, reserved
- `r7`: mouse down, reserved

Registers `r8` through `r31` are free scratch registers. Colors are written with
`out` for normalized `0..1` channels or `out8` for byte `0..255` channels.

See [docs/scope.md](docs/scope.md) and [docs/assembly.md](docs/assembly.md).

