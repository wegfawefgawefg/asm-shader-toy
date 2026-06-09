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

Validate every checked-in example without opening a window:

```sh
./scripts/validate_examples.sh
```

## Run

```sh
./scripts/run.sh
```

Or run a specific program:

```sh
./build/asm-shader-toy examples/basics/plasma.asm --size 240x160 --dimscale 4
```

Image inputs can be loaded into channels:

```sh
./build/asm-shader-toy my.asm --channel0 image.png --channel1 mask.jpg
```

Headless validation:

```sh
./build/asm-shader-toy examples/basics/plasma.asm --dry-run
./build/asm-shader-toy examples/basics/plasma.asm --no-graphics --frames 10
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm \
  --size 506x632 \
  --frames 90 \
  --save-frame /tmp/pixel_planet.png
```

Multi-file programs use `.include` with paths relative to the including file:

```asm
.include "common/math.inc"
.include <std/screen.inc>
```

See [examples/README.md](examples/README.md) for runnable examples covering
time, mouse, image channels, multi-image mixing, includes, and a planet/sphere
visual.

Dedicated multi-file example:

```sh
./build/asm-shader-toy examples/multifile/main.asm
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
- `r7`: mouse down

Registers `r8` through `r15` hold more frame inputs. Built-in names like `px`,
`py`, `time`, `width`, `height`, and `mouse_down` can be used instead of raw
input registers. Scratch registers start at `r16`. Colors are written with
`out` for normalized `0..1` channels or `out8` for byte `0..255` channels.

See [docs/scope.md](docs/scope.md), [docs/assembly.md](docs/assembly.md),
[docs/inputs.md](docs/inputs.md), and
[docs/language-roadmap.md](docs/language-roadmap.md).
