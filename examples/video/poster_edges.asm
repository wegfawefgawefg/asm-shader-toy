; Posterized video with luma edges and a small animated channel split.
; Run with:
; ./build/asm-shader-toy examples/video/poster_edges.asm \
;   --video0 examples/assets/video/big_buck_bunny_4m34s_640x360.mp4 \
;   --size 320x180 \
;   --scale 2

.include <std/screen.inc>

.alias inv_w, r48
.alias inv_h, r49
.alias luma, r50
.alias luma_l, r51
.alias luma_r, r52
.alias luma_u, r53
.alias luma_d, r54
.alias edge, r55
.alias split, r56
.alias scan, r57

div inv_w, 1.0, width
div inv_h, 1.0, height

; Animated RGB split.
mul split, time, 3.0
sin split, split
mul split, split, inv_w
mul split, split, 1.5

sub tmp0, uv_x, split
tex tex0_r, tmp2, tmp3, tex0_a, 0, tmp0, uv_y
tex tmp2, tex0_g, tmp3, tex0_a, 0, uv_x, uv_y
add tmp0, uv_x, split
tex tmp2, tmp3, tex0_b, tex0_a, 0, tmp0, uv_y

; Posterize each channel to chunky bands.
mul color_r, tex0_r, 5.0
floor color_r, color_r
div color_r, color_r, 4.0

mul color_g, tex0_g, 5.0
floor color_g, color_g
div color_g, color_g, 4.0

mul color_b, tex0_b, 5.0
floor color_b, color_b
div color_b, color_b, 4.0

; Current luma.
mul luma, tex0_r, 0.299
mul tmp0, tex0_g, 0.587
add luma, luma, tmp0
mul tmp0, tex0_b, 0.114
add luma, luma, tmp0

; Neighbor luma samples for a cheap edge mask.
sub tmp0, uv_x, inv_w
tex tex1_r, tex1_g, tex1_b, tex1_a, 0, tmp0, uv_y
mul luma_l, tex1_r, 0.299
mul tmp1, tex1_g, 0.587
add luma_l, luma_l, tmp1
mul tmp1, tex1_b, 0.114
add luma_l, luma_l, tmp1

add tmp0, uv_x, inv_w
tex tex1_r, tex1_g, tex1_b, tex1_a, 0, tmp0, uv_y
mul luma_r, tex1_r, 0.299
mul tmp1, tex1_g, 0.587
add luma_r, luma_r, tmp1
mul tmp1, tex1_b, 0.114
add luma_r, luma_r, tmp1

sub tmp0, uv_y, inv_h
tex tex1_r, tex1_g, tex1_b, tex1_a, 0, uv_x, tmp0
mul luma_u, tex1_r, 0.299
mul tmp1, tex1_g, 0.587
add luma_u, luma_u, tmp1
mul tmp1, tex1_b, 0.114
add luma_u, luma_u, tmp1

add tmp0, uv_y, inv_h
tex tex1_r, tex1_g, tex1_b, tex1_a, 0, uv_x, tmp0
mul luma_d, tex1_r, 0.299
mul tmp1, tex1_g, 0.587
add luma_d, luma_d, tmp1
mul tmp1, tex1_b, 0.114
add luma_d, luma_d, tmp1

sub tmp0, luma_l, luma_r
abs tmp0, tmp0
sub tmp1, luma_u, luma_d
abs tmp1, tmp1
add edge, tmp0, tmp1
mul edge, edge, 3.5
min edge, edge, 1.0

; Dark ink at strong edges.
sub tmp0, 1.0, edge
mul color_r, color_r, tmp0
mul color_g, color_g, tmp0
mul color_b, color_b, tmp0

; Subtle horizontal CRT-ish brightness variation.
mul scan, py, 0.5
add scan, scan, time
fract scan, scan
lt scan, scan, 0.5
mul scan, scan, 0.08
sub scan, 1.0, scan

mul color_r, color_r, scan
mul color_g, color_g, scan
mul color_b, color_b, scan

out color_r, color_g, color_b, tex0_a
