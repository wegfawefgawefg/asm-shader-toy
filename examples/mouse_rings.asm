.include "std/screen.inc"

norm tmp0, mouse_x, width
norm tmp1, mouse_y, height
sub tmp2, uv_x, tmp0
sub tmp3, uv_y, tmp1
mul tmp2, tmp2, tmp2
mul tmp3, tmp3, tmp3
add tmp4, tmp2, tmp3
sqrt tmp4, tmp4

mul color_r, tmp4, 32.0
sub color_r, color_r, time
sin color_r, color_r
mul color_r, color_r, 0.5
add color_r, color_r, 0.5
mul color_r, color_r, mouse_down

mul color_g, uv_x, 0.25
add color_g, color_g, 0.05
out color_r, color_g, uv_y, 1.0
