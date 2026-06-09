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

; Planet disk.
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

; Height bands: water, sand shoreline, green land, light-green highland.
gt tmp3, color_a, 0.42
jnz tmp3, sand
mov tmp2, 0.0
jmp cloud_overlay

sand:
gt tmp3, color_a, 0.48
jnz tmp3, land
mov tmp2, 1.0
jmp cloud_overlay

land:
gt tmp3, color_a, 0.68
jnz tmp3, highland
mov tmp2, 2.0
jmp cloud_overlay

highland:
mov tmp2, 3.0
jmp cloud_overlay

cloud_overlay:
; Single-threshold cloud mask. It uses the same rotated sphere point as the
; terrain, but with its own seed/frequency and a slightly faster rotation.
mul tmp3, time, 0.12
add tmp3, tmp12, tmp3
add tmp3, tmp3, 1.71
mul tmp3, tmp3, 8.0
floor tmp5, tmp3

add tmp4, tmp8, 1.43
mul tmp4, tmp4, 8.0
floor tmp9, tmp4

mul tmp10, time, 0.08
add tmp10, tmp14, tmp10
add tmp10, tmp10, 1.29
mul tmp10, tmp10, 8.0
floor tmp10, tmp10

mul color_r, tmp5, 19.0
mul color_g, tmp9, 31.0
add color_r, color_r, color_g
mul color_g, tmp10, 47.0
add color_r, color_r, color_g
mul tmp11, tmp5, tmp9
mul tmp11, tmp11, 13.0
add color_r, color_r, tmp11
mul tmp11, tmp9, tmp10
mul tmp11, tmp11, 17.0
add color_r, color_r, tmp11
mul tmp11, tmp10, tmp5
mul tmp11, tmp11, 23.0
add color_r, color_r, tmp11
add color_r, color_r, 5003.0
mod color_r, color_r, 61.0
div color_r, color_r, 61.0

gt tmp3, color_r, 0.76
jnz tmp3, cloud_color
jmp shade

cloud_color:
out 0.86, 0.90, 0.84, 1.0
ret

planet_shell_overlay:
; Legacy cloud-shell path is disabled while the planet uses the simple
; terrain-attached cloud mask above.
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
; Three-band lighting color table. tmp2 is material:
; 0 water, 1 sand, 2 land, 3 highland.
gt tmp3, tmp15, 0.68
jnz tmp3, shade_bright
gt tmp3, tmp15, 0.28
jnz tmp3, shade_mid
jmp shade_dark

shade_dark:
eq tmp3, tmp2, 0.0
jnz tmp3, water_dark
eq tmp3, tmp2, 1.0
jnz tmp3, sand_dark
eq tmp3, tmp2, 2.0
jnz tmp3, land_dark
jmp highland_dark

shade_mid:
eq tmp3, tmp2, 0.0
jnz tmp3, water_mid
eq tmp3, tmp2, 1.0
jnz tmp3, sand_mid
eq tmp3, tmp2, 2.0
jnz tmp3, land_mid
jmp highland_mid

shade_bright:
eq tmp3, tmp2, 0.0
jnz tmp3, water_bright
eq tmp3, tmp2, 1.0
jnz tmp3, sand_bright
eq tmp3, tmp2, 2.0
jnz tmp3, land_bright
jmp highland_bright

water_dark:
out 0.08, 0.12, 0.32, 1.0
ret
water_mid:
out 0.10, 0.32, 0.62, 1.0
ret
water_bright:
out 0.18, 0.52, 0.86, 1.0
ret

sand_dark:
out 0.24, 0.20, 0.16, 1.0
ret
sand_mid:
out 0.58, 0.50, 0.30, 1.0
ret
sand_bright:
out 0.82, 0.74, 0.45, 1.0
ret

land_dark:
out 0.08, 0.22, 0.18, 1.0
ret
land_mid:
out 0.14, 0.48, 0.28, 1.0
ret
land_bright:
out 0.22, 0.68, 0.36, 1.0
ret

highland_dark:
out 0.18, 0.26, 0.20, 1.0
ret
highland_mid:
out 0.42, 0.62, 0.34, 1.0
ret
highland_bright:
out 0.62, 0.82, 0.45, 1.0
ret
