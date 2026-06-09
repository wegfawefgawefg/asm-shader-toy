; Default demo for asm-shader-toy.
; r0=x, r1=y, r2=time, r3=width, r4=height.

.const pi 3.1415926
.const tau 6.2831853

norm r8, r0, r3       ; u
norm r9, r1, r4       ; v
mul r10, r8, tau
mul r11, r9, tau

add r12, r10, r2
sin r12, r12

mul r13, r2, 0.7
add r13, r11, r13
cos r13, r13

add r14, r12, r13
mul r14, r14, 0.5
add r14, r14, 0.5

mul r15, r8, 0.8
add r15, r15, 0.1
mul r16, r9, 0.6
add r16, r16, 0.2

out r14, r15, r16, 1.0

