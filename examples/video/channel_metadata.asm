.include <std/screen.inc>

.alias ch_w, r48
.alias ch_h, r49
.alias ch_t, r50
.alias sample_u, r51
.alias sample_v, r52
.alias pulse, r53

chdim ch_w, ch_h, 0
chtime ch_t, 0

norm sample_u, px, ch_w
norm sample_v, py, ch_h
tex tex0_r, tex0_g, tex0_b, tex0_a, 0, sample_u, sample_v

mul pulse, ch_t, 6.2831853
sin pulse, pulse
mul pulse, pulse, 0.15
add pulse, pulse, 0.85

mul tex0_r, tex0_r, pulse
mul tex0_g, tex0_g, pulse
mul tex0_b, tex0_b, pulse
out tex0_r, tex0_g, tex0_b, tex0_a
