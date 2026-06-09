; Pixelated rotating planet.
; Adapted for asm-shader-toy from the CC0 "3D Pixelated Planet" Godot shader:
; https://godotshaders.com/shader/3d-pixelated-planet/

.include "../common/math.inc"
.include <std/screen.inc>

; Quantized portrait-friendly screen coordinates.
mul tmp0, pos_x, 0.98
add tmp0, tmp0, 0.02
mul tmp1, pos_y, 1.04
sub tmp1, tmp1, 0.03

mul tmp0, tmp0, 86.0
floor tmp0, tmp0
div tmp0, tmp0, 86.0
mul tmp1, tmp1, 86.0
floor tmp1, tmp1
div tmp1, tmp1, 86.0

; tmp2 = screen radius squared.
mul tmp2, tmp0, tmp0
mul tmp3, tmp1, tmp1
add tmp2, tmp2, tmp3

; Real separate geometry: planet disk inside cloud shell disk.
lt tmp4, tmp2, 0.80
jnz tmp4, planet
lt tmp4, tmp2, 1.02
jnz tmp4, cloud_shell_only
jmp stars

stars:
; Moving unprojected pixel stars. The cells drift slowly over the screen.
mul tmp5, time, 10.0
add tmp5, px, tmp5
mul tmp6, time, 4.0
add tmp6, py, tmp6

div tmp7, tmp5, 19.0
floor tmp7, tmp7
div tmp8, tmp6, 17.0
floor tmp8, tmp8

mul tmp9, tmp7, 12.989
mul tmp10, tmp8, 78.233
add tmp9, tmp9, tmp10
sin tmp9, tmp9
gt tmp9, tmp9, 0.94
jnz tmp9, star_cell_x
out 0.0, 0.0, 0.0, 1.0
ret

star_cell_x:
mod tmp9, tmp5, 19.0
lt tmp9, tmp9, 2.0
jnz tmp9, star_cell_y
out 0.0, 0.0, 0.0, 1.0
ret

star_cell_y:
mod tmp10, tmp6, 17.0
lt tmp10, tmp10, 2.0
jnz tmp10, star
out 0.0, 0.0, 0.0, 1.0
ret

star:
out 0.74, 0.78, 0.82, 1.0
ret

planet:
; Planet sphere point. tmp6=x, tmp7=z/front, tmp8=y.
div tmp6, tmp0, 0.8944
div tmp8, tmp1, 0.8944
mul tmp7, tmp6, tmp6
mul tmp3, tmp8, tmp8
add tmp7, tmp7, tmp3
sub tmp7, 1.0, tmp7
max tmp7, tmp7, 0.0
sqrt tmp7, tmp7

; Rotation around the vertical axis.
mul tmp9, time, 0.45
cos tmp10, tmp9
sin tmp11, tmp9
mul tmp12, tmp6, tmp10
mul tmp13, tmp7, tmp11
add tmp12, tmp12, tmp13
mul tmp13, tmp6, tmp11
mul tmp14, tmp7, tmp10
sub tmp14, tmp14, tmp13

; Upper-right-front light.
mul tmp15, tmp6, 0.55
mul tmp3, tmp8, -0.45
add tmp15, tmp15, tmp3
mul tmp3, tmp7, 0.60
add tmp15, tmp15, tmp3
add tmp15, tmp15, 0.16
max tmp15, tmp15, 0.0
mul tmp15, tmp15, 6.0
floor tmp15, tmp15
div tmp15, tmp15, 6.0

; Terrain: layered sine-noise on rotated sphere coords.
mul color_a, tmp12, 8.0
mul tmp3, tmp8, 5.5
add color_a, color_a, tmp3
sin color_a, color_a
mul color_a, color_a, 0.34

mul tmp3, tmp12, -5.0
mul tmp4, tmp8, 9.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
mul tmp3, tmp3, 0.27
add color_a, color_a, tmp3

mul tmp3, tmp14, 13.0
mul tmp4, tmp8, 3.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
mul tmp3, tmp3, 0.18
add color_a, color_a, tmp3

mul tmp3, tmp12, 21.0
mul tmp4, tmp14, -4.0
add tmp3, tmp3, tmp4
mul tmp4, tmp8, 12.0
add tmp3, tmp3, tmp4
sin tmp3, tmp3
mul tmp3, tmp3, 0.14
add color_a, color_a, tmp3
add color_a, color_a, 0.5

; Base ocean/land/highland.
mov color_r, 0.12
mov color_g, 0.45
mov color_b, 0.78

gt tmp3, color_a, 0.54
jnz tmp3, land
jmp planet_cloud_overlay

land:
mov color_r, 0.17
mov color_g, 0.65
mov color_b, 0.34
gt tmp3, color_a, 0.73
jnz tmp3, highland
jmp planet_cloud_overlay

highland:
mov color_r, 0.58
mov color_g, 0.72
mov color_b, 0.55
jmp planet_cloud_overlay

cloud_shell_only:
; Outside the planet but inside the larger cloud-shell disk.
div tmp6, tmp0, 1.01
div tmp8, tmp1, 1.01
mul tmp7, tmp6, tmp6
mul tmp3, tmp8, tmp8
add tmp7, tmp7, tmp3
sub tmp7, 1.0, tmp7
max tmp7, tmp7, 0.0
sqrt tmp7, tmp7

mul tmp9, time, 0.45
cos tmp10, tmp9
sin tmp11, tmp9

; Cloud shell light.
mul tmp15, tmp6, 0.55
mul tmp3, tmp8, -0.45
add tmp15, tmp15, tmp3
mul tmp3, tmp7, 0.60
add tmp15, tmp15, tmp3
add tmp15, tmp15, 0.16
max tmp15, tmp15, 0.0

jmp cloud_noise_outer

planet_cloud_overlay:
; Same cloud-shell geometry as above, sampled at a larger radius than planet.
div tmp6, tmp0, 1.01
div tmp8, tmp1, 1.01
mul tmp7, tmp6, tmp6
mul tmp3, tmp8, tmp8
add tmp7, tmp7, tmp3
sub tmp7, 1.0, tmp7
max tmp7, tmp7, 0.0
sqrt tmp7, tmp7

mul tmp9, time, 0.45
cos tmp10, tmp9
sin tmp11, tmp9

cloud_noise:
; Quantized shell-cell hash. This avoids obvious sine bands in the clouds.
mul tmp12, tmp6, tmp10
mul tmp13, tmp7, tmp11
add tmp12, tmp12, tmp13
mul tmp13, tmp6, tmp11
mul tmp14, tmp7, tmp10
sub tmp14, tmp14, tmp13

mul tmp3, tmp8, 0.7
add tmp3, tmp3, tmp12
mul tmp3, tmp3, 14.0
floor tmp3, tmp3
mul tmp4, tmp8, -0.35
add tmp4, tmp4, tmp14
mul tmp4, tmp4, 14.0
floor tmp4, tmp4
mul tmp5, tmp12, 0.4
mul tmp13, tmp14, 0.8
add tmp5, tmp5, tmp13
mul tmp5, tmp5, 8.0
floor tmp5, tmp5

mul tmp9, tmp3, 12.989
mul tmp13, tmp4, 78.233
add tmp9, tmp9, tmp13
mul tmp13, tmp5, 37.719
add tmp9, tmp9, tmp13
mul tmp13, time, 2.0
floor tmp13, tmp13
mul tmp13, tmp13, 9.17
add tmp9, tmp9, tmp13
sin tmp9, tmp9

gt tmp3, tmp9, 0.68
jnz tmp3, cloud_candidate
jmp shade

cloud_candidate:
gt tmp3, tmp15, 0.32
jnz tmp3, cloud
jmp shade

cloud_noise_outer:
; For the visible limb shell, use screen-mixed cells to avoid contour rings.
mul tmp3, tmp1, 0.37
add tmp3, tmp3, tmp0
mul tmp3, tmp3, 30.0
floor tmp3, tmp3

mul tmp4, tmp0, -0.61
add tmp4, tmp4, tmp1
mul tmp4, tmp4, 30.0
floor tmp4, tmp4

mul tmp9, tmp3, 12.989
mul tmp13, tmp4, 78.233
add tmp9, tmp9, tmp13
mul tmp13, time, 2.0
floor tmp13, tmp13
mul tmp13, tmp13, 9.17
add tmp9, tmp9, tmp13
sin tmp9, tmp9

gt tmp3, tmp9, 0.78
jnz tmp3, outer_cloud_candidate
jmp stars

outer_cloud_candidate:
gt tmp3, tmp15, 0.50
jnz tmp3, cloud_raw
jmp stars

cloud_raw:
mov color_r, 0.72
mov color_g, 0.76
mov color_b, 0.80
gt tmp3, tmp15, 0.78
jnz tmp3, bright_cloud_raw
out color_r, color_g, color_b, 1.0
ret

bright_cloud_raw:
out 0.92, 0.96, 0.94, 1.0
ret

cloud:
mov color_r, 0.52
mov color_g, 0.56
mov color_b, 0.64
gt tmp3, tmp15, 0.62
jnz tmp3, bright_cloud
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
; Sparse hash stipple near the terminator.
sub tmp3, tmp15, 0.36
abs tmp3, tmp3
lt tmp3, tmp3, 0.12
jnz tmp3, stipple_test
out color_r, color_g, color_b, 1.0
ret

stipple_test:
mul tmp4, px, 17.0
mul tmp5, py, 31.0
add tmp4, tmp4, tmp5
sin tmp4, tmp4
gt tmp4, tmp4, 0.66
jnz tmp4, stipple
out color_r, color_g, color_b, 1.0
ret

stipple:
mul color_r, color_r, 0.72
mul color_g, color_g, 0.72
mul color_b, color_b, 0.72
out color_r, color_g, color_b, 1.0
