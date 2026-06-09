; Video-channel example.
; Run with:
; ./build/asm-shader-toy examples/video/video_channel.asm \
;   --video0 examples/assets/video/big_buck_bunny_1min_160x90_24fps.mp4 \
;   --size 160x90 \
;   --scale 4

.include <std/screen.inc>

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y
out tex0_r, tex0_g, tex0_b, tex0_a
