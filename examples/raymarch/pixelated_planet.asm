; Pixelated rotating planet.
; Adapted for asm-shader-toy from the CC0 "3D Pixelated Planet" Godot shader:
; https://godotshaders.com/shader/3d-pixelated-planet/

.include "../common/math.inc"
.include <std/screen.inc>

; Make a portrait-friendly centered coordinate system and quantize it.
mul tmp0, pos_x, 0.98
add tmp0, tmp0, 0.02
mul tmp1, pos_y, 1.04
sub tmp1, tmp1, 0.03

mul tmp0, tmp0, 84.0
floor tmp0, tmp0
div tmp0, tmp0, 84.0
mul tmp1, tmp1, 84.0
floor tmp1, tmp1
div tmp1, tmp1, 84.0

; tmp2 = radius squared.
mul tmp2, tmp0, tmp0
mul tmp3, tmp1, tmp1
add tmp2, tmp2, tmp3
lt tmp4, tmp2, 0.86
jnz tmp4, planet

; Black space with sparse unprojected pixel stars. No loops: just a cheap
; quantized-cell hash and a tiny local-cell test.
div tmp5, px, 18.0
floor tmp5, tmp5
div tmp6, py, 15.0
floor tmp6, tmp6
mul tmp7, tmp5, 12.989
mul tmp8, tmp6, 78.233
add tmp7, tmp7, tmp8
sin tmp7, tmp7
gt tmp7, tmp7, 0.93
jnz tmp7, star_cell
out 0.0, 0.0, 0.0, 1.0
ret

star_cell:
mod tmp7, px, 18.0
lt tmp7, tmp7, 2.0
jnz tmp7, star_cell_y
out 0.0, 0.0, 0.0, 1.0
ret

star_cell_y:
mod tmp8, py, 15.0
lt tmp8, tmp8, 2.0
jnz tmp8, star
out 0.0, 0.0, 0.0, 1.0
ret

star:
out 0.78, 0.82, 0.86, 1.0
ret

planet:
; Sphere point. tmp6=x, tmp7=z/front, tmp8=y.
div tmp6, tmp0, 0.927
div tmp8, tmp1, 0.927
mul tmp7, tmp6, tmp6
mul tmp3, tmp8, tmp8
add tmp7, tmp7, tmp3
sub tmp7, 1.0, tmp7
max tmp7, tmp7, 0.0
sqrt tmp7, tmp7

; Rotate the sampled terrain around the vertical axis.
mul tmp9, time, 0.45
cos tmp10, tmp9
sin tmp11, tmp9
mul tmp12, tmp6, tmp10
mul tmp13, tmp7, tmp11
add tmp12, tmp12, tmp13
mul tmp13, tmp6, tmp11
mul tmp14, tmp7, tmp10
sub tmp14, tmp14, tmp13

; Light from up/right, leaning slightly toward the camera.
mul tmp15, tmp6, 0.58
mul tmp3, tmp8, -0.42
add tmp15, tmp15, tmp3
mul tmp3, tmp7, 0.52
add tmp15, tmp15, tmp3
add tmp15, tmp15, 0.18
max tmp15, tmp15, 0.0
mul tmp15, tmp15, 6.0
floor tmp15, tmp15
div tmp15, tmp15, 6.0

; Procedural terrain mask from rotated coords.
mul color_a, tmp12, 8.0
mul tmp3, tmp8, 5.5
add color_a, color_a, tmp3
sin color_a, color_a
mul color_a, color_a, 0.38

mul tmp3, tmp12, -5.0
mul tmp4, tmp8, 9.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
mul tmp3, tmp3, 0.32
add color_a, color_a, tmp3

mul tmp3, tmp14, 13.0
mul tmp4, tmp8, 3.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
mul tmp3, tmp3, 0.20
add color_a, color_a, tmp3

mul tmp3, tmp12, 21.0
mul tmp4, tmp14, -4.0
add tmp3, tmp3, tmp4
mul tmp4, tmp8, 12.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
mul tmp3, tmp3, 0.15
add color_a, color_a, tmp3
add color_a, color_a, 0.5

; Base ocean.
mov color_r, 0.13
mov color_g, 0.48
mov color_b, 0.82

gt tmp3, color_a, 0.54
jnz tmp3, land
jmp cloud_test

land:
mov color_r, 0.18
mov color_g, 0.68
mov color_b, 0.38

gt tmp3, color_a, 0.72
jnz tmp3, highland
jmp cloud_test

highland:
mov color_r, 0.62
mov color_g, 0.76
mov color_b, 0.58

cloud_test:
; Separate cloud shell, slightly larger than the planet sphere. It reuses the
; same kind of cheap layered sine noise, but sampled from shell coords.
div tmp3, tmp0, 1.02
div tmp4, tmp1, 1.02
mul tmp5, tmp3, tmp3
mul tmp6, tmp4, tmp4
add tmp5, tmp5, tmp6
sub tmp5, 1.0, tmp5
max tmp5, tmp5, 0.0
sqrt tmp5, tmp5

mul tmp6, tmp3, tmp10
mul tmp7, tmp5, tmp11
add tmp6, tmp6, tmp7
mul tmp7, tmp3, tmp11
mul tmp9, tmp5, tmp10
sub tmp7, tmp9, tmp7

mul tmp9, tmp6, 11.0
mul tmp3, tmp4, 7.0
add tmp9, tmp9, tmp3
mul tmp3, time, 0.45
add tmp9, tmp9, tmp3
sin tmp9, tmp9
mul tmp9, tmp9, 0.42

mul tmp3, tmp7, -8.0
mul tmp4, tmp4, 13.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
mul tmp3, tmp3, 0.34
add tmp9, tmp9, tmp3

mul tmp3, tmp6, 23.0
mul tmp4, tmp7, 5.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
mul tmp3, tmp3, 0.24
add tmp9, tmp9, tmp3
add tmp9, tmp9, 0.5

gt tmp4, tmp9, 0.70
jnz tmp4, cloud_hash_gate
jmp shade

cloud_hash_gate:
mul tmp3, tmp6, 37.0
mul tmp4, tmp7, 17.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
gt tmp3, tmp3, -0.15
jnz tmp3, maybe_cloud
jmp shade

maybe_cloud:
gt tmp4, tmp15, 0.38
jnz tmp4, cloud
jmp shade

cloud:
gt tmp4, tmp15, 0.62
jnz tmp4, bright_cloud
mov color_r, 0.50
mov color_g, 0.54
mov color_b, 0.62
jmp shade

bright_cloud:
mov color_r, 0.90
mov color_g, 0.94
mov color_b, 0.92

shade:
; Night side goes purple/blue, day side goes bright.
gt tmp3, tmp15, 0.22
jnz tmp3, day_side

mul color_r, color_r, 0.16
mul color_g, color_g, 0.13
mul color_b, color_b, 0.30
add color_r, color_r, 0.10
add color_g, color_g, 0.08
add color_b, color_b, 0.16
jmp dither

day_side:
mul color_r, color_r, tmp15
mul color_g, color_g, tmp15
mul color_b, color_b, tmp15
add color_r, color_r, 0.05
add color_g, color_g, 0.07
add color_b, color_b, 0.08

dither:
; Sparse hash stipple near the terminator. This avoids horizontal scan bands.
sub tmp3, tmp15, 0.36
abs tmp3, tmp3
lt tmp3, tmp3, 0.13
jnz tmp3, stipple_test
out color_r, color_g, color_b, 1.0
ret

stipple_test:
mul tmp4, px, 17.0
mul tmp5, py, 31.0
add tmp4, tmp4, tmp5
sin tmp4, tmp4
gt tmp4, tmp4, 0.62
jnz tmp4, stipple
out color_r, color_g, color_b, 1.0
ret

stipple:
mul color_r, color_r, 0.72
mul color_g, color_g, 0.72
mul color_b, color_b, 0.72
out color_r, color_g, color_b, 1.0
