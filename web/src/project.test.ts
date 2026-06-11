import { describe, expect, test } from "vitest";
import { normalizeProject, type ProjectBundle } from "./project";

describe("project bundles", () => {
  test("normalizes missing channel settings for old bundles", () => {
    const project = normalizeProject({
      files: [{ path: "main.asm", content: "out 0.0, 0.0, 0.0, 1.0\n" }],
      settings: {
        main: "main.asm",
        wgsl: "",
        size: "gba",
        scale: 4
      } as ProjectBundle["settings"]
    });

    expect(project.settings.channels).toHaveLength(4);
    expect(project.settings.channels[0]).toMatchObject({ kind: "fallback", name: "channel0", width: 1, height: 1 });
  });

  test("preserves compact generated noise channels", () => {
    const project = normalizeProject({
      files: [{ path: "main.asm", content: "out 0.0, 0.0, 0.0, 1.0\n" }],
      settings: {
        main: "main.asm",
        wgsl: "",
        size: "gba",
        scale: 4,
        channels: [{ kind: "noise", name: "noise:clouds", width: 256, height: 256, seed: "clouds" }]
      }
    });

    expect(project.settings.channels[0]).toMatchObject({
      kind: "noise",
      name: "noise:clouds",
      width: 256,
      height: 256,
      seed: "clouds"
    });
    expect(project.settings.channels[0].imageDataUrl).toBeUndefined();
  });
});
