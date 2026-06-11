# Video Examples

Video channels stream decoded frames with local `ffmpeg` and `ffprobe`, then
sample through the same `tex` instruction used by static image channels.

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

Video channels keep only the current decoded frame in memory. Very large clips
can still be expensive to shade on the CPU, but the runner no longer preloads
the whole decoded video.
