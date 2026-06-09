; Pixelated rotating planet.
; Adapted for asm-shader-toy from the CC0 "3D Pixelated Planet" Godot shader:
; https://godotshaders.com/shader/3d-pixelated-planet/

.include "../common/math.inc"
.include <std/screen.inc>

; Pixelate centered screen coordinates before sphere projection.
mul tmp0, pos_x, 72.0
floor tmp0, tmp0
div tmp0, tmp0, 72.0
mul tmp1, pos_y, 72.0
floor tmp1, tmp1
div tmp1, tmp1, 72.0

; Correct for the default wide-ish window shape and test sphere radius.
norm tmp2, width, height
mul tmp0, tmp0, tmp2
mul tmp3, tmp0, tmp0
mul tmp4, tmp1, tmp1
add tmp3, tmp3, tmp4
lt tmp5, tmp3, 0.72
jnz tmp5, planet

out 0.005, 0.008, 0.025, 1.0
ret

planet:
; Sphere normal from screen disk: nx, ny, nz are tmp6, tmp7, tmp8.
div tmp6, tmp0, 0.848528
div tmp8, tmp1, 0.848528
mul tmp9, tmp6, tmp6
mul tmp10, tmp8, tmp8
add tmp9, tmp9, tmp10
sub tmp7, 1.0, tmp9
max tmp7, tmp7, 0.0
sqrt tmp7, tmp7

; Rotate around the vertical axis.
mul tmp9, time, 0.65
cos tmp10, tmp9
sin tmp11, tmp9
mul tmp12, tmp6, tmp10
mul tmp13, tmp8, tmp11
add tmp12, tmp12, tmp13
mul tmp13, tmp6, tmp11
mul tmp14, tmp8, tmp10
sub tmp14, tmp14, tmp13

; Quantized light from upper-left-front.
mul tmp15, tmp6, -0.25
mul color_a, tmp7, 0.92
add tmp15, tmp15, color_a
mul color_a, tmp8, 0.22
add tmp15, tmp15, color_a
max tmp15, tmp15, 0.0
mul tmp15, tmp15, 8.0
floor tmp15, tmp15
div tmp15, tmp15, 8.0
mul tmp15, tmp15, 1.35

; Procedural terrain value from rotated sphere coords.
mul color_a, tmp12, 9.0
mul tmp13, tmp14, 6.0
add color_a, color_a, tmp13
mul tmp13, tmp7, 3.5
add color_a, color_a, tmp13
sin color_a, color_a
mul color_a, color_a, 0.5
add color_a, color_a, 0.5

; Latitude bands.
mul tmp13, tmp8, 11.0
add tmp13, tmp13, time
sin tmp13, tmp13
mul tmp13, tmp13, 0.5
add tmp13, tmp13, 0.5
mul color_a, color_a, 0.7
mul tmp13, tmp13, 0.3
add color_a, color_a, tmp13

; Ocean by default.
mov color_r, 0.03
mov color_g, 0.22
mov color_b, 0.48

gt tmp13, color_a, 0.48
jnz tmp13, land
jmp shade

land:
mov color_r, 0.16
mov color_g, 0.48
mov color_b, 0.22

gt tmp13, color_a, 0.72
jnz tmp13, highland
jmp shade

highland:
mov color_r, 0.62
mov color_g, 0.55
mov color_b, 0.36

gt tmp13, color_a, 0.88
jnz tmp13, cloud
jmp shade

cloud:
mov color_r, 0.88
mov color_g, 0.9
mov color_b, 0.86

shade:
mul color_r, color_r, tmp15
mul color_g, color_g, tmp15
mul color_b, color_b, tmp15
out color_r, color_g, color_b, 1.0

