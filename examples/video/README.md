# Video Examples

Video channels are not implemented yet.

The repository includes a tiny generated fixture for the future video backend:

```text
examples/assets/video/testsrc_160x90.mp4
```

Intended future command shape:

```sh
./build/asm-shader-toy examples/video/future_video_channel.asm \
  --video0 examples/assets/video/testsrc_160x90.mp4
```

For now, use static image channels with `--channel0` through `--channel3`.

