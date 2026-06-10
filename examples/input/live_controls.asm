.include <std/screen.inc>

; SDL scancodes: A=4, D=7, S=22, W=26, Space=44.
key tmp0, 4
key tmp1, 7
key tmp2, 26
key tmp3, 22
key tmp4, 44

; Mouse buttons: left=0, right=1, middle=2.
mbtn tmp5, 1
mwheel tmp6, tmp7

; SDL controller: A=0, left stick X=0, left stick Y=1.
gbtn tmp8, 0
gaxis tmp9, 0
gaxis tmp10, 1

mul color_r, uv_x, 0.25
mul color_g, uv_y, 0.25
mov color_b, 0.12

add color_r, color_r, tmp1
add color_g, color_g, tmp2
add color_b, color_b, tmp4
add color_r, color_r, tmp5
add color_g, color_g, tmp8

abs tmp9, tmp9
abs tmp10, tmp10
add color_b, color_b, tmp9
add color_r, color_r, tmp10

abs tmp6, tmp6
abs tmp7, tmp7
add color_g, color_g, tmp6
add color_b, color_b, tmp7

out color_r, color_g, color_b, 1.0
