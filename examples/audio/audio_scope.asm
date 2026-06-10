.include <std/screen.inc>

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, 0.25
tex tex1_r, tex1_g, tex1_b, tex1_a, 0, uv_x, 0.75

sub tmp0, uv_y, tex0_r
abs tmp0, tmp0
lt tmp0, tmp0, 0.02

mul color_r, tex1_r, 0.8
add color_r, color_r, tmp0
mul color_g, tex1_r, 0.4
add color_g, color_g, tmp0
mul color_b, tex1_r, 0.15
add color_b, color_b, 0.08

out color_r, color_g, color_b, 1.0
