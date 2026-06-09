.include "common/screen.inc"

.alias mx, r20
.alias my, r21
.alias dx, r22
.alias dy, r23
.alias dist, r24
.alias ring, r25
.alias base, r26

norm mx, mouse_x, width
norm my, mouse_y, height
sub dx, u, mx
sub dy, v, my
mul dx, dx, dx
mul dy, dy, dy
add dist, dx, dy
sqrt dist, dist

mul ring, dist, 32.0
sub ring, ring, time
sin ring, ring
mul ring, ring, 0.5
add ring, ring, 0.5
mul ring, ring, mouse_down

mul base, u, 0.25
add base, base, 0.05
out ring, base, v, 1.0

