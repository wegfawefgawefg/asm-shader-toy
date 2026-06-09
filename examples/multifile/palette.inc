.alias palette_wave, r48

mul palette_wave, time, 1.5
sin palette_wave, palette_wave
mul palette_wave, palette_wave, 0.5
add palette_wave, palette_wave, 0.5

