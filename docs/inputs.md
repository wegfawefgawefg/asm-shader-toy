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

Input aliases are read-only and reserved. Use `.alias name, r16` through
`.alias name, r63` to name scratch registers. Built-in input aliases and aliases
from standard-library includes cannot be redefined.

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
chdim tmp0, tmp1, 0
chtime tmp2, 0
chsrate tmp3, 0
out tex0_r, tex0_g, tex0_b, tex0_a
```

Current behavior:

- channels `0..3`
- PNG/JPEG and any other format supported by local SDL2_image
- video channels through `--video0` through `--video3`, decoded at startup with
  local `ffmpeg` and `ffprobe`
- webcam channels through `--webcam0` through `--webcam3`, streamed with local
  `ffmpeg` and Linux V4L2
- audio channels through `--audio0` through `--audio3`, decoded with local
  `ffmpeg`
- microphone channels through `--mic0` through `--mic3`, captured with local
  `ffmpeg` and PulseAudio
- generated noise channels through `--noise0` through `--noise3`
- feedback buffers through `--buffer0` through `--buffer3`
- normalized `u/v`
- clamped edges
- nearest-neighbor sampling
- sampled color returns normalized `0..1` RGBA floats
- `texel` samples direct pixel coordinates and returns transparent black outside
  the channel bounds
- `chdim` reads channel width and height
- `chtime` reads channel-local time in seconds
- `chsrate` reads channel sample rate
- `key` reads SDL scancode state
- `mbtn` reads mouse button state
- `mwheel` reads current-frame mouse wheel delta
- `gbtn` reads first-gamepad button state
- `gaxis` reads first-gamepad axis state

Buffer N renders into channel N before the image pass:

```sh
./build/asm-shader-toy image.asm --buffer0 feedback.asm
```

Buffer passes sample the previous frame's buffer contents. The image pass samples
the current frame's freshly rendered buffer contents.

Video channels stream decoded frames from an ffmpeg pipe with looping enabled
and keep only the current decoded frame in memory. Very large or high-resolution
files can still be too expensive to shade on the CPU, but they no longer require
preloading the whole decoded clip.

Webcam channels stream `320x240` frames through a background worker and reuse
the latest complete frame when the camera has not produced a newer one.
`--webcam0` defaults to `/dev/video0`; pass a device path after the flag to
override it. Webcam channels are mirrored horizontally by default so shaders can
sample them like normal preview images. Webcam `chtime` reports seconds since
the stream was opened. Video `chtime` reports loop-local playback time.

Audio and microphone channels are `512x2` textures. Row `0` is the waveform,
with samples encoded as unsigned grayscale where `0.5` is silence. Row `1` is a
small spectrum visualization. `chsrate` returns `44100` for these channels.
File audio loops with shader time. Microphone capture updates from a background
pipe and keeps the latest short sample window.

Generated noise channels are static `256x256` RGBA hash-noise textures. Pass an
optional seed after `--noiseN` to get a repeatable variant.

## Runner Controls

Graphical runs reserve only modified app controls:

- `Ctrl+P` toggles pause.
- `Ctrl+R` resets shader time, frame count, and feedback buffers.
- `Escape` exits.

Plain `P`, `R`, space, arrows, mouse buttons, and gamepad buttons remain visible
to shaders through the live input query instructions.

## Live Input Queries

Keyboard, mouse, and gamepad state are queried with instructions instead of
fixed registers:

```asm
key tmp0, 4          ; SDL_SCANCODE_A
mbtn tmp1, 1         ; right mouse button
mwheel tmp2, tmp3    ; wheel x/y delta for this frame
gbtn tmp4, 0         ; first controller A button
gaxis tmp5, 0        ; first controller left stick X
```

Common SDL scancodes are `4` A, `7` D, `22` S, `26` W, `40` Return, `41`
Escape, `44` Space, `79` Right, `80` Left, `81` Down, and `82` Up.

Mouse buttons are `0` left, `1` right, `2` middle, `3` X1, and `4` X2. Mouse
wheel values are deltas for the current rendered frame.

Gamepad button and axis indices follow SDL2's `SDL_GameController` enums for
the first attached controller. Common buttons are `0` A, `1` B, `2` X, `3` Y,
`4` Back, `6` Start, `9` left shoulder, `10` right shoulder, and `11..14`
D-pad up/down/left/right. Common axes are `0` left X, `1` left Y, `2` right X,
`3` right Y, `4` left trigger, and `5` right trigger.

## Expansion Notes

A Shadertoy-compatible keyboard texture is still a possible follow-up for
porting existing shaders, but direct input query instructions are the preferred
assembly-facing path for now.

There is a tiny generated video fixture:

```text
examples/assets/video/testsrc_160x90.mp4
```

See `examples/video/video_channel.asm` for a runnable video channel example.

The important constraint is that channel sampling should stay explicit. A fake
assembly program should make memory and texture access visible instead of
pretending it is normal arithmetic.
