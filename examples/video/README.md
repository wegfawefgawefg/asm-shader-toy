# Video Examples

Video channels are decoded at startup with local `ffmpeg` and `ffprobe`, then
sampled through the same `tex` instruction used by static image channels.

The repository includes a tiny generated fixture:

```text
examples/assets/video/testsrc_160x90.mp4
```

It also includes a more interesting Big Buck Bunny clip:

```text
examples/assets/video/big_buck_bunny_4m34s_640x360.mp4
```

Run it with:

```sh
./build/asm-shader-toy examples/video/video_channel.asm \
  --video0 examples/assets/video/big_buck_bunny_4m34s_640x360.mp4 \
  --size 320x180 \
  --scale 2
```

Or run the stylized poster/edge version:

```sh
./build/asm-shader-toy examples/video/poster_edges.asm \
  --video0 examples/assets/video/big_buck_bunny_4m34s_640x360.mp4 \
  --size 320x180 \
  --scale 2
```

Video frames are currently decoded and preloaded at startup. That keeps sampling
simple, but it is not appropriate for full movies yet.
