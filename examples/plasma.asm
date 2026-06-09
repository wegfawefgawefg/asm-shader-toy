; Default demo for asm-shader-toy.
; Built-in aliases: px, py, time, width, height.

.const pi 3.1415926
.const tau 6.2831853
.alias u, r16
.alias v, r17
.alias wave_x, r18
.alias wave_y, r19
.alias plasma, r20
.alias red, r21
.alias green, r22

norm u, px, width
norm v, py, height
mul wave_x, u, tau
mul wave_y, v, tau

add plasma, wave_x, time
sin plasma, plasma

mul wave_y, time, 0.7
add wave_y, v, wave_y
cos wave_y, wave_y

add plasma, plasma, wave_y
mul plasma, plasma, 0.5
add plasma, plasma, 0.5

mul red, u, 0.8
add red, red, 0.1
mul green, v, 0.6
add green, green, 0.2

out plasma, red, green, 1.0
