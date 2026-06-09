.include "common/screen.inc"

.alias cr, r20
.alias cg, r21
.alias cb, r22
.alias ca, r23

tex cr, cg, cb, ca, 0, u, v
out cr, cg, cb, ca

