#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

"${repo_root}/scripts/build.sh"

cd "${repo_root}"

./build/asm-shader-toy examples/plasma.asm --dry-run
./build/asm-shader-toy examples/time_pulse.asm --dry-run
./build/asm-shader-toy examples/mouse_rings.asm --dry-run
./build/asm-shader-toy examples/planet_sphere.asm --dry-run
./build/asm-shader-toy examples/image_passthrough.asm --channel0 examples/assets/checker.png --dry-run
./build/asm-shader-toy examples/multi_image_mix.asm \
    --channel0 examples/assets/checker.png \
    --channel1 examples/assets/bars.png \
    --dry-run

./build/asm-shader-toy examples/plasma.asm --no-graphics --frames 2
./build/asm-shader-toy examples/time_pulse.asm --no-graphics --frames 2
./build/asm-shader-toy examples/mouse_rings.asm --no-graphics --frames 2
./build/asm-shader-toy examples/planet_sphere.asm --no-graphics --frames 2
./build/asm-shader-toy examples/image_passthrough.asm \
    --channel0 examples/assets/checker.png \
    --no-graphics \
    --frames 2
./build/asm-shader-toy examples/multi_image_mix.asm \
    --channel0 examples/assets/checker.png \
    --channel1 examples/assets/bars.png \
    --no-graphics \
    --frames 2

