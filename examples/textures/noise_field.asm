.include <std/screen.inc>

mul tmp0, time, 0.03
add tmp1, uv_x, tmp0
add tmp2, uv_y, tmp0
fract tmp1, tmp1
fract tmp2, tmp2

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp1, tmp2

mul color_r, tex0_r, uv_x
mul color_g, tex0_g, uv_y
mul color_b, tex0_b, 0.8

out color_r, color_g, color_b, 1.0
