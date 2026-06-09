.include "common/screen.inc"

.alias ar, r20
.alias ag, r21
.alias ab, r22
.alias aa, r23
.alias br, r24
.alias bg, r25
.alias bb, r26
.alias ba, r27
.alias mixv, r28
.alias inv, r29
.alias red, r30
.alias green, r31
.alias blue, r32

tex ar, ag, ab, aa, 0, u, v
tex br, bg, bb, ba, 1, u, v

mul mixv, time, 0.7
sin mixv, mixv
mul mixv, mixv, 0.5
add mixv, mixv, 0.5
sub inv, 1.0, mixv

mul red, ar, inv
mul br, br, mixv
add red, red, br

mul green, ag, inv
mul bg, bg, mixv
add green, green, bg

mul blue, ab, inv
mul bb, bb, mixv
add blue, blue, bb

out red, green, blue, 1.0

