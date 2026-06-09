.alias u, r16
.alias v, r17
.alias cx, r18
.alias cy, r19

norm u, px, width
norm v, py, height
mul cx, u, 2.0
sub cx, cx, 1.0
mul cy, v, 2.0
sub cy, cy, 1.0

