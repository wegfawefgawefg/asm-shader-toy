; Heavy CPU benchmark shader.
; This is intentionally instruction-heavy for VM/runtime profiling.

.include "../common/math.inc"
.include <std/screen.inc>

mul tmp0, pos_x, pos_x
mul tmp1, pos_y, pos_y
add tmp2, tmp0, tmp1
sqrt tmp2, tmp2

mul tmp3, pos_x, 7.0
mul tmp4, pos_y, 5.0
add tmp3, tmp3, tmp4
add tmp3, tmp3, time
sin tmp3, tmp3

mul tmp4, pos_x, -3.0
mul tmp5, pos_y, 11.0
add tmp4, tmp4, tmp5
mul tmp5, time, 0.71
add tmp4, tmp4, tmp5
cos tmp4, tmp4

add color_r, tmp3, tmp4
mul color_r, color_r, 0.25
add color_r, color_r, 0.5

mul tmp5, pos_x, 19.0
add tmp5, tmp5, 17.0
floor tmp5, tmp5
mul tmp6, pos_y, 23.0
add tmp6, tmp6, 13.0
floor tmp6, tmp6
mul tmp7, tmp5, 17.0
mul tmp8, tmp6, 31.0
add tmp7, tmp7, tmp8
mul tmp8, tmp5, tmp6
mul tmp8, tmp8, 7.0
add tmp7, tmp7, tmp8
add tmp7, tmp7, 4096.0
mod tmp7, tmp7, 37.0
div tmp7, tmp7, 37.0

mul tmp8, tmp2, 18.0
add tmp8, tmp8, time
sin tmp8, tmp8
mul tmp8, tmp8, 0.5
add tmp8, tmp8, 0.5
mul color_g, tmp7, 0.55
mul tmp9, tmp8, 0.45
add color_g, color_g, tmp9

mul tmp9, pos_x, 41.0
mul tmp10, pos_y, -29.0
add tmp9, tmp9, tmp10
mul tmp10, time, 1.3
add tmp9, tmp9, tmp10
sin tmp9, tmp9

mul tmp10, pos_x, -13.0
mul tmp11, pos_y, 37.0
add tmp10, tmp10, tmp11
mul tmp11, time, 0.9
sub tmp10, tmp10, tmp11
cos tmp10, tmp10

mul tmp11, tmp9, tmp10
abs tmp11, tmp11
sqrt tmp11, tmp11
mul color_b, tmp11, 0.65
mul tmp12, color_r, 0.35
add color_b, color_b, tmp12

mul tmp12, color_r, color_g
mul tmp13, color_b, 0.25
add tmp12, tmp12, tmp13
gt tmp13, tmp12, 0.58
jnz tmp13, bright

mul color_r, color_r, 0.62
mul color_g, color_g, 0.70
mul color_b, color_b, 0.88
add color_b, color_b, 0.06
out color_r, color_g, color_b, 1.0
ret

bright:
mul color_r, color_r, 0.80
add color_r, color_r, 0.16
mul color_g, color_g, 0.88
add color_g, color_g, 0.10
mul color_b, color_b, 0.72
add color_b, color_b, 0.18
out color_r, color_g, color_b, 1.0
ret
