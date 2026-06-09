.include <std/screen.inc>

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y

mul color_r, tex0_r, 0.72
mul color_g, tex0_r, 1.0
mul color_b, tex0_r, 0.62
out color_r, color_g, color_b, 1.0
