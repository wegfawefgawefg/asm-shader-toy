.include <std/screen.inc>

main:
    mul tmp0, uv_x, 8.0
    floor tmp0, tmp0
    call palette
    out color_r, color_g, color_b, 1.0
    halt

palette:
    jlt tmp0, 2.0, .blue
    jlt tmp0, 5.0, .green
    jmp .pink

.blue:
    mov color_r, 0.10
    mov color_g, 0.25
    mov color_b, 0.85
    ret

.green:
    mov color_r, 0.10
    mov color_g, 0.80
    mov color_b, 0.45
    ret

.pink:
    mov color_r, 0.95
    mov color_g, 0.30
    mov color_b, 0.60
    ret

