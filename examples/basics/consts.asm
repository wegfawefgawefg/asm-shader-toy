.consts
    mov pi, 3.14159265
    add tau, pi, pi
    mov blue_r, 0.08
    mov blue_g, 0.20
    mov blue_b, 0.85
    mov gold_r, 0.95
    mov gold_g, 0.72
    mov gold_b, 0.18
.end

.include <std/screen.inc>

mul tmp0, uv_x, tau
add tmp0, tmp0, time
sin tmp0, tmp0
gt tmp1, tmp0, 0.0
jnz tmp1, gold

out blue_r, blue_g, blue_b, 1.0
halt

gold:
out gold_r, gold_g, gold_b, 1.0
halt
