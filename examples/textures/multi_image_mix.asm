.include <std/screen.inc>

; Left side shows channel 0, right side shows channel 1, and the center blends.

mul tmp2, uv_x, 2.0
fract tmp2, tmp2
tex tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp2, uv_y
tex tex1_r, tex1_g, tex1_b, tex1_a, 1, tmp2, uv_y

mul tmp0, time, 0.7
sin tmp0, tmp0
mul tmp0, tmp0, 0.5
add tmp0, tmp0, 0.5

lt tmp3, uv_x, 0.4
jnz tmp3, left

gt tmp3, uv_x, 0.6
jnz tmp3, right

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
ret

left:
out tex0_r, tex0_g, tex0_b, tex0_a
ret

right:
out tex1_r, tex1_g, tex1_b, tex1_a
