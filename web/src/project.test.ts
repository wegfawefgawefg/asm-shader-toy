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
    expect(project.settings.buffers).toEqual([null, null, null, null]);
  });

  test("preserves browser feedback buffer slots", () => {
    const project = normalizeProject({
      files: [
        { path: "main.asm", content: "out 0.0, 0.0, 0.0, 1.0\n" },
        { path: "buffer.asm", content: "out 1.0, 1.0, 1.0, 1.0\n" }
      ],
      settings: {
        main: "main.asm",
        wgsl: "",
        size: "gba",
        scale: 4,
        channels: [],
        buffers: [{ file: "buffer.asm", wgsl: "compiled" }]
      }
    });

    expect(project.settings.buffers).toHaveLength(4);
    expect(project.settings.buffers?.[0]).toEqual({ file: "buffer.asm", wgsl: "compiled" });
    expect(project.settings.buffers?.[1]).toBeNull();
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

  test("preserves browser webcam channel metadata without serializing streams", () => {
    const project = normalizeProject({
      files: [{ path: "main.asm", content: "out 0.0, 0.0, 0.0, 1.0\n" }],
      settings: {
        main: "main.asm",
        wgsl: "",
        size: "gba",
        scale: 4,
        channels: [{ kind: "webcam", name: "webcam", width: 640, height: 480 }]
      }
    });

    expect(project.settings.channels[0]).toMatchObject({
      kind: "webcam",
      name: "webcam",
      width: 640,
      height: 480
    });
  });

  test("preserves browser microphone channel metadata without serializing streams", () => {
    const project = normalizeProject({
      files: [{ path: "main.asm", content: "out 0.0, 0.0, 0.0, 1.0\n" }],
      settings: {
        main: "main.asm",
        wgsl: "",
        size: "gba",
        scale: 4,
        channels: [{ kind: "microphone", name: "microphone", width: 512, height: 2, sampleRate: 48000 }]
      }
    });

    expect(project.settings.channels[0]).toMatchObject({
      kind: "microphone",
      name: "microphone",
      width: 512,
      height: 2,
      sampleRate: 48000
    });
  });

  test("preserves browser video channel metadata without serializing media blobs", () => {
    const project = normalizeProject({
      files: [{ path: "main.asm", content: "out 0.0, 0.0, 0.0, 1.0\n" }],
      settings: {
        main: "main.asm",
        wgsl: "",
        size: "gba",
        scale: 4,
        channels: [{ kind: "video", name: "clip.mp4", width: 640, height: 360 }]
      }
    });

    expect(project.settings.channels[0]).toMatchObject({
      kind: "video",
      name: "clip.mp4",
      width: 640,
      height: 360
    });
  });

  test("preserves browser audio channel metadata without serializing media blobs", () => {
    const project = normalizeProject({
      files: [{ path: "main.asm", content: "out 0.0, 0.0, 0.0, 1.0\n" }],
      settings: {
        main: "main.asm",
        wgsl: "",
        size: "gba",
        scale: 4,
        channels: [{ kind: "audio", name: "loop.wav", width: 512, height: 2, sampleRate: 44100 }]
      }
    });

    expect(project.settings.channels[0]).toMatchObject({
      kind: "audio",
      name: "loop.wav",
      width: 512,
      height: 2,
      sampleRate: 44100
    });
  });
});
