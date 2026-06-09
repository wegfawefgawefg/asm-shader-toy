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

; Black space with sparse diagonal streaks.
mul tmp5, pos_x, 11.0
mul tmp6, pos_y, 19.0
add tmp5, tmp5, tmp6
sub tmp5, tmp5, time
sin tmp5, tmp5
gt tmp5, tmp5, 0.935
jnz tmp5, star_gate
out 0.0, 0.0, 0.0, 1.0
ret

star_gate:
mul tmp6, pos_x, 41.0
mul tmp7, pos_y, -7.0
add tmp6, tmp6, tmp7
sin tmp6, tmp6
gt tmp6, tmp6, 0.82
jnz tmp6, star
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

; Light from the right/front. Quantized for chunky bands.
mul tmp15, tmp6, 0.78
mul tmp3, tmp7, 0.42
add tmp15, tmp15, tmp3
add tmp15, tmp15, 0.26
max tmp15, tmp15, 0.0
mul tmp15, tmp15, 7.0
floor tmp15, tmp15
div tmp15, tmp15, 7.0

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
mul tmp3, tmp3, 0.25
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
; Bright day-side cloud masses, mostly on the right.
gt tmp4, tmp15, 0.91
jnz tmp4, cloud

mul tmp3, tmp12, 10.0
mul tmp4, tmp8, 8.0
add tmp3, tmp3, tmp4
add tmp3, tmp3, time
sin tmp3, tmp3
gt tmp4, tmp3, 0.42
jnz tmp4, maybe_cloud
jmp shade

maybe_cloud:
gt tmp4, tmp15, 0.5
jnz tmp4, cloud
jmp shade

cloud:
mov color_r, 0.92
mov color_g, 0.96
mov color_b, 0.94

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
; Vertical-ish stipple near the terminator.
sub tmp3, tmp15, 0.42
abs tmp3, tmp3
lt tmp3, tmp3, 0.20
jnz tmp3, stipple_test
out color_r, color_g, color_b, 1.0
ret

stipple_test:
mod tmp4, py, 4.0
lt tmp4, tmp4, 1.0
jnz tmp4, stipple
out color_r, color_g, color_b, 1.0
ret

stipple:
mul color_r, color_r, 0.72
mul color_g, color_g, 0.72
mul color_b, color_b, 0.72
out color_r, color_g, color_b, 1.0
