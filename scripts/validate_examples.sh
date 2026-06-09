#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

"${repo_root}/scripts/build.sh"

cd "${repo_root}"

./build/asm-shader-toy examples/basics/plasma.asm --dry-run
./build/asm-shader-toy examples/basics/time_pulse.asm --dry-run
./build/asm-shader-toy examples/input/mouse_rings.asm --dry-run
./build/asm-shader-toy examples/multifile/main.asm --dry-run
./build/asm-shader-toy examples/perf/heavy.asm --dry-run
./build/asm-shader-toy examples/raymarch/planet_sphere.asm --dry-run
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm --dry-run
./build/asm-shader-toy examples/textures/image_passthrough.asm --channel0 examples/assets/checker.png --dry-run
./build/asm-shader-toy examples/textures/multi_image_mix.asm \
    --channel0 examples/assets/checker.png \
    --channel1 examples/assets/bars.png \
    --dry-run

./build/asm-shader-toy examples/basics/plasma.asm --no-graphics --frames 2
./build/asm-shader-toy examples/basics/time_pulse.asm --no-graphics --frames 2
./build/asm-shader-toy examples/input/mouse_rings.asm --no-graphics --frames 2
./build/asm-shader-toy examples/multifile/main.asm --no-graphics --frames 2
./build/asm-shader-toy examples/perf/heavy.asm --no-graphics --frames 2
./build/asm-shader-toy examples/raymarch/planet_sphere.asm --no-graphics --frames 2
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm --no-graphics --frames 2
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm \
    --size 64x80 \
    --frames 2 \
    --save-frame /tmp/asm-shader-toy-pixelated-planet.png
./build/asm-shader-toy examples/textures/image_passthrough.asm \
    --channel0 examples/assets/checker.png \
    --no-graphics \
    --frames 2
./build/asm-shader-toy examples/textures/multi_image_mix.asm \
    --channel0 examples/assets/checker.png \
    --channel1 examples/assets/bars.png \
    --no-graphics \
    --frames 2
