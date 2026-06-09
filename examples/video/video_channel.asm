; Video-channel example.
; Run with:
; ./build/asm-shader-toy examples/video/video_channel.asm \
;   --video0 examples/assets/video/big_buck_bunny_4m34s_640x360.mp4 \
;   --size 320x180 \
;   --scale 2

.include <std/screen.inc>

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y
out tex0_r, tex0_g, tex0_b, tex0_a
