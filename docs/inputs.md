# Inputs

This project borrows the shape of Shadertoy's standard inputs but maps them to
registers instead of GLSL uniforms.

## Shadertoy Reference Shape

Common Shadertoy fragment programs receive:

- `iResolution`: viewport resolution in pixels.
- `iTime`: playback time in seconds.
- `iTimeDelta`: time for the previous frame.
- `iFrame`: current playback frame.
- `iMouse`: mouse coordinates and click coordinates.
- `iDate`: year, month, day, and seconds within the day.
- `iChannel0` through `iChannel3`: image, buffer, video, sound, webcam, cubemap,
  keyboard, or other input channels.
- `iChannelTime[4]`: playback time per channel.
- `iChannelResolution[4]`: resolution per channel.
- `iSampleRate`: audio sample rate when sound is involved.

## Current Register Map

These are available today:

```text
r0   pixel x
r1   pixel y
r2   iTime
r3   iResolution.x
r4   iResolution.y
r5   iMouse.x while left button is down, otherwise 0
r6   iMouse.y while left button is down, otherwise 0
r7   mouse down, 1 or 0
r8   iMouse.z click x
r9   iMouse.w click y
r10  iFrame
r11  iTimeDelta
r12  iDate.w, local seconds since midnight
r13  iDate.x, local year
r14  iDate.y, local month
r15  iDate.z, local day
```

Scratch registers start at `r16`.

## Expansion Plan

Near-term scalar inputs:

- aspect ratio helper, probably `r16` only if we add named aliases or more
  registers
- keyboard state as a compact channel or key query instruction
- pause/reset controls for stable experiments
- mouse wheel and right/middle buttons

First channel support:

- one static image loaded with `--channel0 path.png`
- nearest and linear sampling modes
- wrap and clamp address modes
- instructions like `tex dst_r, dst_g, dst_b, dst_a, channel, u, v`
- channel metadata registers or query instructions for width, height, time

Later channel support:

- four channels matching Shadertoy's `iChannel0..3`
- previous-frame buffer channel for feedback effects
- video file channel
- generated noise texture channel
- keyboard texture channel compatible with common Shadertoy keyboard examples
- audio FFT/waveform channel
- webcam or microphone only if the platform path stays boring

The important constraint is that channel sampling should stay explicit. A fake
assembly program should make memory and texture access visible instead of
pretending it is normal arithmetic.

