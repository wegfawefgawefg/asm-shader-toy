; Video-channel example.
; Run with:
; ./build/asm-shader-toy examples/video/video_channel.asm \
;   --video0 examples/assets/video/testsrc_160x90.mp4 \
;   --size 160x90 \
;   --scale 4

.include <std/screen.inc>

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y

; Add a simple time-varying scanline so video playback timing is visible.
mul tmp0, uv_y, 90.0
add tmp0, tmp0, time
fract tmp0, tmp0
lt tmp1, tmp0, 0.5
mul tmp1, tmp1, 0.18
sub tmp2, 1.0, tmp1

mul color_r, tex0_r, tmp2
mul color_g, tex0_g, tmp2
mul color_b, tex0_b, tmp2
out color_r, color_g, color_b, tex0_a
