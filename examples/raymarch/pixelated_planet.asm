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

; Planet disk. Clouds are hidden while terrain noise is being tuned.
lt tmp4, tmp2, 0.80
jnz tmp4, planet
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

; Terrain: three octaves of cheap 3D lattice noise in rotated sphere space.
; Nearest cells keep the pixel-art edge; no sine bands and no 2D polar seam.
mov color_a, 0.0

; Octave 1, broad continental masses.
add tmp3, tmp12, 1.31
mul tmp3, tmp3, 3.0
floor tmp5, tmp3

add tmp4, tmp8, 1.17
mul tmp4, tmp4, 3.0
floor tmp9, tmp4

add tmp10, tmp14, 1.03
mul tmp10, tmp10, 3.0
floor tmp10, tmp10

mul color_r, tmp5, 17.0
mul color_g, tmp9, 29.0
add color_r, color_r, color_g
mul color_g, tmp10, 43.0
add color_r, color_r, color_g
mul tmp11, tmp5, tmp9
mul tmp11, tmp11, 11.0
add color_r, color_r, tmp11
mul tmp11, tmp9, tmp10
mul tmp11, tmp11, 19.0
add color_r, color_r, tmp11
mul tmp11, tmp10, tmp5
mul tmp11, tmp11, 23.0
add color_r, color_r, tmp11
add color_r, color_r, 4096.0
mod color_r, color_r, 47.0
div color_r, color_r, 47.0
mul color_r, color_r, 0.55
add color_a, color_a, color_r

; Octave 2, coastline breakup.
add tmp3, tmp12, 1.31
mul tmp3, tmp3, 7.0
floor tmp5, tmp3

add tmp4, tmp8, 1.17
mul tmp4, tmp4, 7.0
floor tmp9, tmp4

add tmp10, tmp14, 1.03
mul tmp10, tmp10, 7.0
floor tmp10, tmp10

mul color_r, tmp5, 23.0
mul color_g, tmp9, 11.0
add color_r, color_r, color_g
mul color_g, tmp10, 37.0
add color_r, color_r, color_g
mul tmp11, tmp5, tmp9
mul tmp11, tmp11, 17.0
add color_r, color_r, tmp11
mul tmp11, tmp9, tmp10
mul tmp11, tmp11, 13.0
add color_r, color_r, tmp11
mul tmp11, tmp10, tmp5
mul tmp11, tmp11, 29.0
add color_r, color_r, tmp11
add color_r, color_r, 4101.0
mod color_r, color_r, 53.0
div color_r, color_r, 53.0
mul color_r, color_r, 0.30
add color_a, color_a, color_r

; Octave 3, small island and edge detail.
add tmp3, tmp12, 1.31
mul tmp3, tmp3, 14.0
floor tmp5, tmp3

add tmp4, tmp8, 1.17
mul tmp4, tmp4, 14.0
floor tmp9, tmp4

add tmp10, tmp14, 1.03
mul tmp10, tmp10, 14.0
floor tmp10, tmp10

mul color_r, tmp5, 31.0
mul color_g, tmp9, 19.0
add color_r, color_r, color_g
mul color_g, tmp10, 41.0
add color_r, color_r, color_g
mul tmp11, tmp5, tmp9
mul tmp11, tmp11, 23.0
add color_r, color_r, tmp11
mul tmp11, tmp9, tmp10
mul tmp11, tmp11, 31.0
add color_r, color_r, tmp11
mul tmp11, tmp10, tmp5
mul tmp11, tmp11, 17.0
add color_r, color_r, tmp11
add color_r, color_r, 4117.0
mod color_r, color_r, 59.0
div color_r, color_r, 59.0
mul color_r, color_r, 0.15
add color_a, color_a, color_r

; Slight front-side bias keeps the visible face from being all ocean.
mul tmp9, tmp14, -0.04
add color_a, color_a, tmp9

; Base ocean/land/highland.
mov color_r, 0.12
mov color_g, 0.45
mov color_b, 0.78

gt tmp3, color_a, 0.42
jnz tmp3, land
jmp shade

land:
mov color_r, 0.17
mov color_g, 0.65
mov color_b, 0.34
gt tmp3, color_a, 0.62
jnz tmp3, highland
jmp shade

highland:
mov color_r, 0.58
mov color_g, 0.72
mov color_b, 0.55
jmp shade

planet_shell_overlay:
; Hidden for now. Keep the label around so the cloud shell can be restored
; against the same terrain noise path later.
jmp shade
mov tmp2, 1.0
jmp compute_cloud_shell

cloud_shell_only:
; Outside the planet but inside the larger cloud-shell disk.
mov tmp2, 0.0

compute_cloud_shell:
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

cloud_noise_outer:
; Project one cloud shell over the full larger sphere, then punch it into
; sparse cells. Hash inputs are shifted positive before mod so negative
; screen coordinates do not pass every threshold.
mul tmp3, tmp1, 0.43
add tmp3, tmp3, tmp0
mul tmp3, tmp3, 11.0
floor tmp3, tmp3

mul tmp4, tmp0, -0.58
add tmp4, tmp4, tmp1
mul tmp4, tmp4, 11.0
floor tmp4, tmp4

mul tmp9, tmp3, 5.0
mul tmp13, tmp4, 11.0
add tmp9, tmp9, tmp13
mul tmp13, time, 2.0
floor tmp13, tmp13
mul tmp13, tmp13, 5.0
add tmp9, tmp9, tmp13
add tmp9, tmp9, 4096.0
mod tmp9, tmp9, 23.0

lt tmp5, tmp9, 9.0
jnz tmp5, outer_cloud_gate2
jnz tmp2, shade
jmp stars

outer_cloud_gate2:
mul tmp9, tmp3, 13.0
mul tmp13, tmp4, 7.0
add tmp9, tmp9, tmp13
add tmp9, tmp9, 4096.0
mod tmp9, tmp9, 29.0
lt tmp5, tmp9, 16.0
jnz tmp5, outer_cloud_gate3
jnz tmp2, shade
jmp stars

outer_cloud_gate3:
mul tmp9, tmp1, 0.21
add tmp9, tmp9, tmp0
mul tmp9, tmp9, 32.0
floor tmp9, tmp9

mul tmp13, tmp0, -0.77
add tmp13, tmp13, tmp1
mul tmp13, tmp13, 32.0
floor tmp13, tmp13

mul tmp9, tmp9, 17.0
mul tmp13, tmp13, 19.0
add tmp9, tmp9, tmp13
add tmp9, tmp9, 4096.0
mod tmp9, tmp9, 31.0
gt tmp5, tmp9, 4.0
jnz tmp5, outer_cloud_candidate
jnz tmp2, shade
jmp stars

outer_cloud_candidate:
gt tmp3, tmp15, 0.24
jnz tmp3, cloud_raw
jnz tmp2, shade
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
