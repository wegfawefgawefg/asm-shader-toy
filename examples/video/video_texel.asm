; Exact texel video-channel example for CPU/GPU parity checks.
; Run with:
; ./build/asm-shader-toy examples/video/video_texel.asm \
;   --video0 examples/assets/video/testsrc_160x90.mp4 \
;   --size 160x90 \
;   --scale 2

.include <std/screen.inc>

texel tex0_r, tex0_g, tex0_b, tex0_a, 0, px, py
out tex0_r, tex0_g, tex0_b, tex0_a
