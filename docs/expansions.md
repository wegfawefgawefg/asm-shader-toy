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

5. Assembler-time const blocks. Done.

   `.consts ... .end` runs compile-time asm in a const environment and exports
   assigned slots as constants. It reuses the shared opcode executor for all
   non-runtime ops instead of adding an infix expression language.

6. Channel dimensions. Done.

   Add a query instruction such as `chdim dw, dh, channel` so shaders can adapt
   to image, video, webcam, and buffer dimensions without hard-coded values.

7. Channel time. Done.

   Add a query instruction or input mapping for per-channel playback time. This
   mainly matters for video and future audio channels.

8. Texture sampling modes. Dismissed.

   Keep `tex` nearest-neighbor and clamp-to-edge for now. Shadertoy exposes
   channel sampling controls, but this project can keep the default small and
   let shaders use `fract`, `min`, and `max` when they want custom wrapping or
   clamping behavior.

9. Streaming video decode. Done.

   `--videoN` now streams raw frames from an ffmpeg pipe with looping enabled
   and keeps only the current decoded frame instead of preloading the whole
   clip.

10. Threaded live input capture. Done.

    `--webcamN` now drains its nonblocking ffmpeg pipe on a background worker
    thread. The render loop copies the latest complete frame into the channel
    snapshot instead of doing capture work inline.

11. Keyboard and gamepad input. Done.

    Added `key dst, scancode` for SDL scancodes plus `gbtn dst, button` and
    `gaxis dst, axis` for the first SDL game controller. A future
    Shadertoy-compatible keyboard texture can still be added if porting
    examples needs it.

12. More mouse inputs. Done.

    Added `mbtn dst, button` for left/right/middle/X buttons and `mwheel dx,
    dy` for current-frame wheel delta.

13. Pause and reset controls. Done.

    Added `Ctrl+P` pause/resume and `Ctrl+R` reset in graphical runs. Plain
    keys remain available to shaders through `key`.

14. Audio channel input. Done.

    Added `--audioN path`, decoded through ffmpeg into a `512x2` waveform and
    spectrum texture. Added `chsrate dst, channel` for sample-rate metadata.

15. Microphone input. Done.

    Added `--micN [device]`, captured through ffmpeg/PulseAudio into the same
    `512x2` live audio texture shape.

16. Generated noise channel. Done.

    Added `--noiseN [seed]`, a static `256x256` RGBA hash-noise texture channel.

17. Macro-like conveniences.

    Consider macros only after subroutines and local labels are in place. The
    useful version would probably be tiny: named instruction blocks or repeated
    boilerplate, not a full preprocessor.
