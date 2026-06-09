.include "../common/math.inc"
.include <std/screen.inc>

mul tmp0, time, 2.0
add tmp0, tmp0, pos_x
sin tmp0, tmp0
mul tmp0, tmp0, 0.5
add tmp0, tmp0, 0.5

mul tmp1, pos_x, pos_x
mul color_b, pos_y, pos_y
add tmp1, tmp1, color_b
sqrt tmp1, tmp1
mul tmp1, tmp1, 12.0
sub tmp1, tmp1, time
sin tmp1, tmp1
mul tmp1, tmp1, 0.5
add tmp1, tmp1, 0.5

mul color_r, tmp0, 0.9
mul color_g, tmp1, 0.8
mul color_b, tmp0, tmp1
out color_r, color_g, color_b, 1.0
