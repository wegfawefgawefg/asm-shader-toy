#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

"${repo_root}/scripts/build.sh"

cd "${repo_root}"

./build/asm-shader-toy examples/basics/consts.asm --dry-run
./build/asm-shader-toy examples/basics/plasma.asm --dry-run
./build/asm-shader-toy examples/basics/subroutines.asm --dry-run
./build/asm-shader-toy examples/basics/subroutines.asm \
    --emit-wgsl /tmp/asm-shader-toy-subroutines.wgsl
grep -q "call_stack" /tmp/asm-shader-toy-subroutines.wgsl
./build/asm-shader-toy examples/basics/time_pulse.asm --dry-run
./build/asm-shader-toy examples/basics/time_pulse.asm --emit-wgsl /tmp/asm-shader-toy-time-pulse.wgsl
grep -q "@compute @workgroup_size" /tmp/asm-shader-toy-time-pulse.wgsl
./build/asm-shader-toy examples/audio/audio_scope.asm \
    --audio0 examples/assets/audio/two_tone.wav \
    --dry-run
./build/asm-shader-toy examples/input/live_controls.asm --dry-run
./build/asm-shader-toy examples/input/live_controls.asm \
    --emit-wgsl /tmp/asm-shader-toy-live-controls.wgsl
grep -q "ast_key_state" /tmp/asm-shader-toy-live-controls.wgsl
./build/asm-shader-toy examples/input/mouse_rings.asm --dry-run
./build/asm-shader-toy examples/buffers/life_display.asm \
    --buffer0 examples/buffers/life_buffer.asm \
    --dry-run
./build/asm-shader-toy examples/multifile/main.asm --dry-run
./build/asm-shader-toy examples/perf/heavy.asm --dry-run
./build/asm-shader-toy examples/raymarch/planet_sphere.asm --dry-run
./build/asm-shader-toy examples/raymarch/pixelated_planet.asm --dry-run
./build/asm-shader-toy examples/textures/image_passthrough.asm --channel0 examples/assets/checker.png --dry-run
./build/asm-shader-toy examples/textures/image_passthrough.asm \
    --emit-wgsl /tmp/asm-shader-toy-image-passthrough.wgsl
grep -q "channel0_texture" /tmp/asm-shader-toy-image-passthrough.wgsl
./build/asm-shader-toy examples/textures/multi_image_mix.asm \
    --channel0 examples/assets/checker.png \
    --channel1 examples/assets/bars.png \
    --dry-run
./build/asm-shader-toy examples/textures/noise_field.asm --noise0 42 --dry-run
./build/asm-shader-toy examples/video/video_channel.asm \
    --video0 examples/assets/video/testsrc_160x90.mp4 \
    --dry-run
./build/asm-shader-toy examples/video/poster_edges.asm \
    --video0 examples/assets/video/testsrc_160x90.mp4 \
    --dry-run
./build/asm-shader-toy examples/video/channel_metadata.asm \
    --video0 examples/assets/video/testsrc_160x90.mp4 \
    --dry-run

./build/asm-shader-toy examples/basics/consts.asm --no-graphics --frames 2
./build/asm-shader-toy examples/basics/plasma.asm --no-graphics --frames 2
./build/asm-shader-toy examples/basics/subroutines.asm --no-graphics --frames 2
./build/asm-shader-toy examples/basics/time_pulse.asm --no-graphics --frames 2
./build/asm-shader-toy examples/audio/audio_scope.asm \
    --audio0 examples/assets/audio/two_tone.wav \
    --no-graphics \
    --frames 4
./build/asm-shader-toy examples/input/live_controls.asm --no-graphics --frames 2
./build/asm-shader-toy examples/input/mouse_rings.asm --no-graphics --frames 2
./build/asm-shader-toy examples/buffers/life_display.asm \
    --buffer0 examples/buffers/life_buffer.asm \
    --no-graphics \
    --frames 4
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
./build/asm-shader-toy examples/textures/noise_field.asm \
    --noise0 42 \
    --no-graphics \
    --frames 2
./build/asm-shader-toy examples/video/video_channel.asm \
    --video0 examples/assets/video/testsrc_160x90.mp4 \
    --size 160x90 \
    --no-graphics \
    --frames 4
./build/asm-shader-toy examples/video/poster_edges.asm \
    --video0 examples/assets/video/testsrc_160x90.mp4 \
    --size 160x90 \
    --no-graphics \
    --frames 4
./build/asm-shader-toy examples/video/channel_metadata.asm \
    --video0 examples/assets/video/testsrc_160x90.mp4 \
    --size 160x90 \
    --no-graphics \
    --frames 4

(
    cd "${repo_root}/web"
    AST_NATIVE_CLI="${repo_root}/build/asm-shader-toy" npm test
    npm run build
)
