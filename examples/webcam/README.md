# Webcam Examples

Webcam channels use local `ffmpeg` with Linux V4L2. `--webcam0` defaults to
`/dev/video0`; `--webcam1` defaults to `/dev/video1`.

Run a mirrored preview:

```sh
./build/asm-shader-toy examples/webcam/webcam_channel.asm \
  --webcam0 \
  --size 320x240 \
  --scale 2
```

Run the stylized video effect on the webcam:

```sh
./build/asm-shader-toy examples/video/poster_edges.asm \
  --webcam0 \
  --size 320x240 \
  --scale 2
```

Use an explicit device path if needed:

```sh
./build/asm-shader-toy examples/webcam/webcam_channel.asm \
  --webcam0 /dev/video1 \
  --size 320x240 \
  --scale 2
```

The current webcam capture size is fixed at `320x240`.
