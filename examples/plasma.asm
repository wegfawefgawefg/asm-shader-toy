; Default demo for asm-shader-toy.
; Built-in aliases: px, py, time, width, height.

.include "common/math.inc"
.include "std/screen.inc"

mul tmp0, uv_x, tau
mul tmp1, uv_y, tau

add color_r, tmp0, time
sin color_r, color_r

mul tmp1, time, 0.7
add tmp1, uv_y, tmp1
cos tmp1, tmp1

add color_r, color_r, tmp1
mul color_r, color_r, 0.5
add color_r, color_r, 0.5

mul color_g, uv_x, 0.8
add color_g, color_g, 0.1
mul color_b, uv_y, 0.6
add color_b, color_b, 0.2

out color_r, color_g, color_b, 1.0
