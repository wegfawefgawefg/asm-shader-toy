.include <std/aliases.inc>

; Conway's Game of Life feedback buffer.
; Channel 0 is the previous frame of this buffer. Cells outside the image are
; dead.

.alias sum, r50
.alias alive, r51
.alias born, r52
.alias two_neighbors, r53
.alias survive, r54
.alias next_alive, r55
.alias mouse_dx, r56
.alias mouse_dy, r57
.alias brush, r58
.alias hash, r59
.alias rand_alive, r60
.alias sample_alive, r61

mov sum, 0.0

; Top row.
sub tmp0, px, 1.0
sub tmp1, py, 1.0
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, tmp1
gt sample_alive, tex0_r, 0.5
add sum, sum, sample_alive

mov tmp0, px
sub tmp1, py, 1.0
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, tmp1
gt sample_alive, tex0_r, 0.5
add sum, sum, sample_alive

add tmp0, px, 1.0
sub tmp1, py, 1.0
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, tmp1
gt sample_alive, tex0_r, 0.5
add sum, sum, sample_alive

; Middle row.
sub tmp0, px, 1.0
mov tmp1, py
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, tmp1
gt sample_alive, tex0_r, 0.5
add sum, sum, sample_alive

add tmp0, px, 1.0
mov tmp1, py
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, tmp1
gt sample_alive, tex0_r, 0.5
add sum, sum, sample_alive

; Bottom row.
sub tmp0, px, 1.0
add tmp1, py, 1.0
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, tmp1
gt sample_alive, tex0_r, 0.5
add sum, sum, sample_alive

mov tmp0, px
add tmp1, py, 1.0
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, tmp1
gt sample_alive, tex0_r, 0.5
add sum, sum, sample_alive

add tmp0, px, 1.0
add tmp1, py, 1.0
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, tmp0, tmp1
gt sample_alive, tex0_r, 0.5
add sum, sum, sample_alive

; Center cell rule.
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, px, py
gt alive, tex0_r, 0.5
eq born, sum, 3.0
eq two_neighbors, sum, 2.0
mul survive, alive, two_neighbors
max next_alive, born, survive

; Mouse brush: fill a small disk with cheap deterministic random cells.
sub mouse_dx, px, mouse_x
sub mouse_dy, py, mouse_y
mul tmp0, mouse_dx, mouse_dx
mul tmp1, mouse_dy, mouse_dy
add tmp0, tmp0, tmp1
lt brush, tmp0, 90.0
mul brush, brush, mouse_down

mul hash, px, 12.9898
mul tmp0, py, 78.233
add hash, hash, tmp0
mul tmp0, frame, 0.117
add hash, hash, tmp0
sin hash, hash
mul hash, hash, 43758.5453
fract hash, hash

gt rand_alive, hash, 0.86
lt tmp1, frame, 1.0
mul rand_alive, rand_alive, tmp1
max next_alive, next_alive, rand_alive

gt rand_alive, hash, 0.68
mul rand_alive, rand_alive, brush
max next_alive, next_alive, rand_alive

out next_alive, next_alive, next_alive, 1.0
