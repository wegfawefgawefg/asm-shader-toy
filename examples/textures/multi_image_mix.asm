.include <std/screen.inc>

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y
tex tex1_r, tex1_g, tex1_b, tex1_a, 1, uv_x, uv_y

mul tmp0, time, 0.7
sin tmp0, tmp0
mul tmp0, tmp0, 0.5
add tmp0, tmp0, 0.5
sub tmp1, 1.0, tmp0

mul color_r, tex0_r, tmp1
mul tex1_r, tex1_r, tmp0
add color_r, color_r, tex1_r

mul color_g, tex0_g, tmp1
mul tex1_g, tex1_g, tmp0
add color_g, color_g, tex1_g

mul color_b, tex0_b, tmp1
mul tex1_b, tex1_b, tmp0
add color_b, color_b, tex1_b

out color_r, color_g, color_b, 1.0
