# Video Assets

`testsrc_160x90.mp4` is a tiny generated FFmpeg test pattern for video channel
work.

`big_buck_bunny_1min_160x90_24fps.mp4` is a small, audio-free one-minute
transcode of the Wikimedia Commons file `Big_Buck_Bunny_medium.ogv`.

The runner supports video channels through `--video0` through `--video3` when
local `ffmpeg` and `ffprobe` executables are available.

## Attribution

Big Buck Bunny clip:

- Source: https://commons.wikimedia.org/wiki/File:Big_Buck_Bunny_medium.ogv
- Original author: (c) copyright Blender Foundation | www.bigbuckbunny.org
- License: Creative Commons Attribution 3.0 Unported
- Local changes: first 60 seconds transcoded to MP4, scaled to 160x90, encoded
  at 24 FPS, and audio removed for use as a small shader input fixture.
