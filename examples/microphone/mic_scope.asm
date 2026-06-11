.include <std/screen.inc>

; Live microphone scope. Run with --mic0.

chsrate tmp8, 0
div tmp8, tmp8, 44100.0
min tmp8, tmp8, 1.0

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, 0.25
tex tex1_r, tex1_g, tex1_b, tex1_a, 0, uv_x, 0.75

sub tmp0, tex0_r, 0.5
abs tmp0, tmp0
mul tmp0, tmp0, 2.8
min tmp0, tmp0, 1.0

sub tmp1, uv_y, tex0_r
abs tmp1, tmp1
lt tmp1, tmp1, 0.025

mul color_r, tex1_r, 0.55
add color_r, color_r, tmp0
mul color_g, tmp0, 0.75
add color_g, color_g, tmp1
mul color_b, tex1_r, 0.35
add color_b, color_b, tmp8

out color_r, color_g, color_b, 1.0
