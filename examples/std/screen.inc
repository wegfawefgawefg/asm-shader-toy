.include "aliases.inc"

norm uv_x, px, width
norm uv_y, py, height
mul pos_x, uv_x, 2.0
sub pos_x, pos_x, 1.0
mul pos_y, uv_y, 2.0
sub pos_y, pos_y, 1.0

