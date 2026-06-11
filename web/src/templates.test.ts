import { describe, expect, test } from "vitest";
import { templateProjects, makeTemplateProject } from "./templates";

describe("template projects", () => {
  test("includes every asm example as a selectable template", () => {
    expect(templateProjects.map((template) => template.id)).toEqual([
      "examples/basics/consts.asm",
      "examples/basics/plasma.asm",
      "examples/basics/subroutines.asm",
      "examples/basics/time_pulse.asm",
      "examples/input/live_controls.asm",
      "examples/input/mouse_rings.asm",
      "examples/buffers/life_display.asm",
      "examples/buffers/ramp_display.asm",
      "examples/multifile/main.asm",
      "examples/perf/heavy.asm",
      "examples/raymarch/planet_sphere.asm",
      "examples/raymarch/pixelated_planet.asm",
      "examples/textures/image_passthrough.asm",
      "examples/textures/multi_image_mix.asm",
      "examples/textures/noise_field.asm",
      "examples/audio/audio_scope.asm",
      "examples/microphone/mic_scope.asm",
      "examples/webcam/webcam_channel.asm",
      "examples/video/video_channel.asm",
      "examples/video/video_texel.asm",
      "examples/video/channel_metadata.asm",
      "examples/video/poster_edges.asm"
    ]);
  });

  test("builds multifile and feedback buffer templates", () => {
    const multifile = makeTemplateProject(templateProjects.find((template) => template.id === "examples/multifile/main.asm")!);
    expect(multifile.files.map((file) => file.path)).toContain("examples/multifile/palette.inc");
    expect(multifile.settings.wgsl).toContain("textureStore");

    const life = makeTemplateProject(templateProjects.find((template) => template.id === "examples/buffers/life_display.asm")!);
    expect(life.settings.buffers?.[0]?.file).toBe("examples/buffers/life_buffer.asm");
  });
});
