# Video Assets

`testsrc_160x90.mp4` is a tiny generated FFmpeg test pattern for video channel
work.

`big_buck_bunny_4m34s_640x360.mp4` is a small, audio-free 6-second transcode
from the middle of the Wikimedia Commons file `Big_Buck_Bunny_medium.ogv`.

The runner supports video channels through `--video0` through `--video3` when
local `ffmpeg` and `ffprobe` executables are available.

## Attribution

Big Buck Bunny clip:

- Source: https://commons.wikimedia.org/wiki/File:Big_Buck_Bunny_medium.ogv
- Original author: (c) copyright Blender Foundation | www.bigbuckbunny.org
- License: Creative Commons Attribution 3.0 Unported
- Local changes: 6 seconds starting around 4:34 transcoded to MP4, scaled to
  640x360, encoded at 24 FPS, and audio removed for use as a small shader input
  fixture.
