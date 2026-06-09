; Webcam channel example.
; Run with:
; ./build/asm-shader-toy examples/webcam/webcam_channel.asm \
;   --webcam0 \
;   --size 320x240 \
;   --scale 2

.include <std/screen.inc>

; Mirror horizontally for a more natural webcam preview.
sub tmp0, 1.0, uv_x
tex tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, uv_y
out tex0_r, tex0_g, tex0_b, tex0_a
