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

  test("lets pixelated planet use default render settings", () => {
    const pixelated = makeTemplateProject(
      templateProjects.find((template) => template.id === "examples/raymarch/pixelated_planet.asm")!
    );

    expect(pixelated.settings.size).toBe("gb");
    expect(pixelated.settings.scale).toBe(4);
    expect(pixelated.settings.maxSteps).toBe(4096);
    expect(pixelated.settings.wgsl).toContain("steps >= 4096");
    expect(pixelated.settings.wgsl.match(/^        case /gm)?.length ?? 0).toBeLessThan(120);
  });

  test("preloads useful channel assets for media templates", () => {
    const image = makeTemplateProject(templateProjects.find((template) => template.id === "examples/textures/image_passthrough.asm")!);
    expect(image.settings.channels[0]).toMatchObject({
      kind: "image",
      name: "checker.png",
      sourceUrl: "examples/assets/checker.png"
    });

    const multiImage = makeTemplateProject(templateProjects.find((template) => template.id === "examples/textures/multi_image_mix.asm")!);
    expect(multiImage.settings.channels[0].sourceUrl).toBe("examples/assets/checker.png");
    expect(multiImage.settings.channels[1].sourceUrl).toBe("examples/assets/bars.png");

    const audio = makeTemplateProject(templateProjects.find((template) => template.id === "examples/audio/audio_scope.asm")!);
    expect(audio.settings.channels[0]).toMatchObject({
      kind: "audio",
      name: "two_tone.wav",
      sourceUrl: "examples/assets/audio/two_tone.wav"
    });

    const video = makeTemplateProject(templateProjects.find((template) => template.id === "examples/video/poster_edges.asm")!);
    expect(video.settings.channels[0]).toMatchObject({
      kind: "video",
      name: "big_buck_bunny_4m34s_640x360.mp4",
      sourceUrl: "examples/assets/video/big_buck_bunny_4m34s_640x360.mp4"
    });
  });
});
