# Microphone Examples

Microphone channels use local `ffmpeg` with PulseAudio. `--mic0` defaults to
the PulseAudio `default` input. Pass a source name after the flag to override
it.

Run a live scope:

```sh
./build/asm-shader-toy examples/microphone/mic_scope.asm \
  --mic0 \
  --size 320x180 \
  --scale 2
```

Use an explicit PulseAudio source if needed:

```sh
./build/asm-shader-toy examples/microphone/mic_scope.asm \
  --mic0 alsa_input.pci-0000_00_1f.3.analog-stereo \
  --size 320x180 \
  --scale 2
```

Audio and microphone channels are `512x2` textures. Row `0` is waveform and row
`1` is a small spectrum. Use `chsrate dst, channel` to read the sample rate.
