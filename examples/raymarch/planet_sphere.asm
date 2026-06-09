.include "../common/math.inc"
.include <std/screen.inc>

norm tmp0, width, height
mul tmp1, pos_x, tmp0
mov tmp2, pos_y

mul tmp3, tmp1, tmp1
mul tmp4, tmp2, tmp2
add tmp3, tmp3, tmp4
lt tmp5, tmp3, 0.72
jnz tmp5, shade

out 0.01, 0.015, 0.04, 1.0
ret

shade:
sub tmp6, 0.72, tmp3
sqrt tmp7, tmp6

mov tmp8, tmp1
mov tmp9, tmp2
mov tmp10, tmp7

mul tmp11, tmp8, -0.35
mul tmp12, tmp9, -0.25
add tmp11, tmp11, tmp12
mul tmp12, tmp10, 0.9
add tmp11, tmp11, tmp12
max tmp11, tmp11, 0.0

mul tmp12, tmp2, 10.0
add tmp12, tmp12, time
sin tmp12, tmp12
mul tmp12, tmp12, 0.5
add tmp12, tmp12, 0.5

mul tmp13, tmp8, 7.0
add tmp13, tmp13, time
sin tmp13, tmp13
mul tmp13, tmp13, 0.5
add tmp13, tmp13, 0.5

mul tmp14, tmp8, 13.0
add tmp14, tmp14, tmp2
add tmp14, tmp14, time
sin tmp14, tmp14
mul tmp14, tmp14, 0.5
add tmp14, tmp14, 0.5

mul color_r, tmp12, 0.25
add color_r, color_r, 0.05
mul color_g, tmp13, 0.45
add color_g, color_g, 0.15
mul color_b, tmp13, 0.75
add color_b, color_b, tmp14
mul color_b, color_b, 0.45

mul color_r, color_r, tmp11
mul color_g, color_g, tmp11
mul color_b, color_b, tmp11
out color_r, color_g, color_b, 1.0
