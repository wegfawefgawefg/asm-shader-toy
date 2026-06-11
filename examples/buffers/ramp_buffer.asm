.include <std/screen.inc>

; Deterministic feedback buffer used by the native WebGPU parity gate.
; The red channel accumulates over frames while green/blue preserve coordinates.

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y
add color_r, tex0_r, 0.125
fract color_r, color_r
out color_r, uv_x, uv_y, 1.0
