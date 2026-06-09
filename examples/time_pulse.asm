.include "common/math.inc"
.include "common/screen.inc"

.alias wave, r20
.alias rings, r21
.alias red, r22
.alias green, r23
.alias blue, r24

mul wave, time, 2.0
add wave, wave, cx
sin wave, wave
mul wave, wave, 0.5
add wave, wave, 0.5

mul rings, cx, cx
mul blue, cy, cy
add rings, rings, blue
sqrt rings, rings
mul rings, rings, 12.0
sub rings, rings, time
sin rings, rings
mul rings, rings, 0.5
add rings, rings, 0.5

mul red, wave, 0.9
mul green, rings, 0.8
mul blue, wave, rings
out red, green, blue, 1.0

