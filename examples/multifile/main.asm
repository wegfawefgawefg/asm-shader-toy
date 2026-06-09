.include <std/screen.inc>
.include "palette.inc"
.include "rings.inc"

mul color_r, palette_wave, ring_dist
mul color_g, uv_x, palette_wave
mul color_b, uv_y, ring_dist
out color_r, color_g, color_b, 1.0

