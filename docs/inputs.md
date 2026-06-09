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

The same inputs also have source aliases:

```text
px, pixel_x, py, pixel_y, time, width, height,
mouse_x, mouse_y, mouse_down, mouse_click_x, mouse_click_y,
frame, time_delta, wall_seconds, date_year, date_month, date_day
```

Input aliases are read-only. Use `.alias name, r16` through `.alias name, r63`
to name scratch registers.

## Channels

Up to four static image, video, or webcam inputs can be loaded:

```sh
./build/asm-shader-toy program.asm --channel0 albedo.png --channel1 mask.jpg
./build/asm-shader-toy program.asm --video0 clip.mp4
./build/asm-shader-toy program.asm --webcam0
```

Sampling is explicit:

```asm
.include <std/screen.inc>

tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y
out tex0_r, tex0_g, tex0_b, tex0_a
```

Current behavior:

- channels `0..3`
- PNG/JPEG and any other format supported by local SDL2_image
- video channels through `--video0` through `--video3`, decoded at startup with
  local `ffmpeg` and `ffprobe`
- webcam channels through `--webcam0` through `--webcam3`, streamed with local
  `ffmpeg` and Linux V4L2
- feedback buffers through `--buffer0` through `--buffer3`
- normalized `u/v`
- clamped edges
- nearest-neighbor sampling
- sampled color returns normalized `0..1` RGBA floats
- `texel` samples direct pixel coordinates and returns transparent black outside
  the channel bounds

Buffer N renders into channel N before the image pass:

```sh
./build/asm-shader-toy image.asm --buffer0 feedback.asm
```

Buffer passes sample the previous frame's buffer contents. The image pass samples
the current frame's freshly rendered buffer contents.

Video channels currently preload all decoded frames. This is simple and works
well for short fixtures, but streaming decode is needed before using long movies
or high-resolution clips.

Webcam channels stream `320x240` frames through a nonblocking pipe and reuse the
latest complete frame when the camera has not produced a newer one. `--webcam0`
defaults to `/dev/video0`; pass a device path after the flag to override it.

## Expansion Plan

Near-term scalar inputs:

- aspect ratio helper, probably `r16` only if we add named aliases or more
  registers
- keyboard state as a compact channel or key query instruction
- pause/reset controls for stable experiments
- mouse wheel and right/middle buttons

Next channel support:

- streaming video decode instead of preloading all frames
- linear sampling mode
- wrap address mode
- channel metadata registers or query instructions for width, height, time

Later channel support:

- generated noise texture channel
- keyboard texture channel compatible with common Shadertoy keyboard examples
- audio FFT/waveform channel
- microphone only if the platform path stays boring

There is a tiny generated video fixture:

```text
examples/assets/video/testsrc_160x90.mp4
```

See `examples/video/video_channel.asm` for a runnable video channel example.

The important constraint is that channel sampling should stay explicit. A fake
assembly program should make memory and texture access visible instead of
pretending it is normal arithmetic.
