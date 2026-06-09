.include "common/math.inc"
.include "common/screen.inc"

.alias aspect, r20
.alias sx, r21
.alias sy, r22
.alias d2, r23
.alias inside, r24
.alias z2, r25
.alias z, r26
.alias nx, r27
.alias ny, r28
.alias nz, r29
.alias light, r30
.alias bands, r31
.alias ocean, r32
.alias clouds, r33
.alias red, r34
.alias green, r35
.alias blue, r36
.alias tmp, r37

norm aspect, width, height
mul sx, cx, aspect
mov sy, cy

mul d2, sx, sx
mul tmp, sy, sy
add d2, d2, tmp
lt inside, d2, 0.72
jnz inside, shade

out 0.01, 0.015, 0.04, 1.0
ret

shade:
sub z2, 0.72, d2
sqrt z, z2

mov nx, sx
mov ny, sy
mov nz, z

mul light, nx, -0.35
mul tmp, ny, -0.25
add light, light, tmp
mul tmp, nz, 0.9
add light, light, tmp
max light, light, 0.0

mul bands, sy, 10.0
add bands, bands, time
sin bands, bands
mul bands, bands, 0.5
add bands, bands, 0.5

mul ocean, nx, 7.0
add ocean, ocean, time
sin ocean, ocean
mul ocean, ocean, 0.5
add ocean, ocean, 0.5

mul clouds, nx, 13.0
add clouds, clouds, sy
add clouds, clouds, time
sin clouds, clouds
mul clouds, clouds, 0.5
add clouds, clouds, 0.5

mul red, bands, 0.25
add red, red, 0.05
mul green, ocean, 0.45
add green, green, 0.15
mul blue, ocean, 0.75
add blue, blue, clouds
mul blue, blue, 0.45

mul red, red, light
mul green, green, light
mul blue, blue, light
out red, green, blue, 1.0

