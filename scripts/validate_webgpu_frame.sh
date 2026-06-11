#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
frame_tool="${repo_root}/build-webgpu-probe/ast-webgpu-frame"

"${repo_root}/scripts/build_webgpu_probe.sh"
"${repo_root}/build-webgpu-probe/ast-webgpu-probe"

run_case() {
    local name="$1"
    shift
    echo "webgpu-frame: ${name}"
    "${frame_tool}" "$@" --compare-cpu
}

run_case "consts" \
    "${repo_root}/examples/basics/consts.asm" \
    --size 32x24

run_case "subroutines" \
    "${repo_root}/examples/basics/subroutines.asm" \
    --size 32x24

run_case "time_pulse" \
    "${repo_root}/examples/basics/time_pulse.asm" \
    --size 32x24 \
    --time 1.25 \
    --frame 75

run_case "mouse_rings" \
    "${repo_root}/examples/input/mouse_rings.asm" \
    --size 32x24

run_case "plasma" \
    "${repo_root}/examples/basics/plasma.asm" \
    --size 64x48

run_case "heavy" \
    "${repo_root}/examples/perf/heavy.asm" \
    --size 40x40 \
    --time 0.5 \
    --frame 30 \
    --tolerance 2

run_case "planet_sphere" \
    "${repo_root}/examples/raymarch/planet_sphere.asm" \
    --size 48x48 \
    --time 0.5 \
    --frame 30 \
    --tolerance 2
