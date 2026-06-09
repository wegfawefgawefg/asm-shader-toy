.alias ring_dist, r49
.alias ring_tmp, r50

mul ring_dist, pos_x, pos_x
mul ring_tmp, pos_y, pos_y
add ring_dist, ring_dist, ring_tmp
sqrt ring_dist, ring_dist
mul ring_dist, ring_dist, 18.0
sub ring_dist, ring_dist, time
sin ring_dist, ring_dist
mul ring_dist, ring_dist, 0.5
add ring_dist, ring_dist, 0.5

