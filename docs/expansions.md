# Expansion List

This is the numbered list of language and runtime features that still look
worth serious consideration. The intent is to pick through these one by one.

1. Include-once by default. Done.

   Resolve every `.include` to a canonical path and include that file only once
   per assembled program. Repeated textual inclusion mostly creates duplicate
   labels, aliases, and constants. If repeated inclusion becomes useful later,
   add an explicit raw include directive instead of making it the default.

2. Subroutines. Done.

   Add `call`, real subroutine `ret`, and `halt`. `call` should push a return
   address onto a small bounded VM call stack. `ret` should return from a call,
   and either halt or error when the stack is empty. `halt` gives programs an
   unambiguous end instruction.

3. Branch ergonomics. Done.

   Add a small set of direct branch helpers if the current `cmp-ish op` plus
   `jnz` pattern stays annoying. Candidates are `jz`, `jeq`, `jne`, `jlt`,
   `jle`, `jgt`, and `jge`. Keep this small so control flow remains visible.

4. Local labels. Done.

   Support labels scoped under the previous global label, such as `.loop` or
   `.done`. This would make includes and helper blocks easier to write without
   inventing globally unique label names everywhere.

5. Constant expressions.

   Let `.const` evaluate simple arithmetic expressions, for example
   `TAU = PI * 2`. Useful operators would be `+`, `-`, `*`, `/`, parentheses,
   and references to earlier constants.

6. Channel dimensions.

   Add a query instruction such as `chdim dw, dh, channel` so shaders can adapt
   to image, video, webcam, and buffer dimensions without hard-coded values.

7. Channel time.

   Add a query instruction or input mapping for per-channel playback time. This
   mainly matters for video and future audio channels.

8. Texture sampling modes.

   Add explicit control for nearest versus linear sampling, and clamp versus
   wrap or mirror addressing. Nearest/clamp should remain the default because it
   matches the pixel-art/debugging posture.

9. Streaming video decode.

   `--videoN` currently preloads all decoded frames. Replace that with a small
   ring buffer fed on demand before encouraging arbitrary long videos.

10. Threaded live input capture.

    `--webcamN` currently drains a nonblocking pipe during the app loop. Moving
    capture into a background thread with a small ring buffer would isolate
    rendering from camera or decoder stalls.

11. Keyboard input.

    Add either a compact keyboard channel, a key query instruction, or both.
    A Shadertoy-compatible keyboard texture would make porting examples easier,
    but a direct query instruction may be nicer assembly.

12. More mouse inputs.

    Add mouse wheel and right/middle button state. Current inputs cover only
    left-button position and click position.

13. Pause and reset controls.

    Useful for stable experiments, feedback buffers, cellular automata, and
    debugging time-dependent shaders.

14. Audio channel input.

    Add waveform and FFT channel support if the platform path stays simple
    enough. `iSampleRate` should arrive with this.

15. Microphone input.

    Treat microphone as optional follow-up to audio channel support. It is only
    worth doing if capture can stay boring across supported platforms.

16. Generated noise channel.

    A built-in noise texture channel could make examples cheaper and easier,
    but it should not hide ordinary arithmetic or texture access from learners.

17. Macro-like conveniences.

    Consider macros only after subroutines and local labels are in place. The
    useful version would probably be tiny: named instruction blocks or repeated
    boilerplate, not a full preprocessor.
