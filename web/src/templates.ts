import { compileAsmToWgsl } from "./compiler";
import { normalizeProject, type ProjectBundle, type ProjectFile } from "./project";

import commonMath from "../../examples/common/math.inc?raw";
import consts from "../../examples/basics/consts.asm?raw";
import plasma from "../../examples/basics/plasma.asm?raw";
import subroutines from "../../examples/basics/subroutines.asm?raw";
import timePulse from "../../examples/basics/time_pulse.asm?raw";
import liveControls from "../../examples/input/live_controls.asm?raw";
import mouseRings from "../../examples/input/mouse_rings.asm?raw";
import lifeDisplay from "../../examples/buffers/life_display.asm?raw";
import lifeBuffer from "../../examples/buffers/life_buffer.asm?raw";
import rampDisplay from "../../examples/buffers/ramp_display.asm?raw";
import rampBuffer from "../../examples/buffers/ramp_buffer.asm?raw";
import multifileMain from "../../examples/multifile/main.asm?raw";
import multifilePalette from "../../examples/multifile/palette.inc?raw";
import multifileRings from "../../examples/multifile/rings.inc?raw";
import heavy from "../../examples/perf/heavy.asm?raw";
import planetSphere from "../../examples/raymarch/planet_sphere.asm?raw";
import pixelatedPlanet from "../../examples/raymarch/pixelated_planet.asm?raw";
import imagePassthrough from "../../examples/textures/image_passthrough.asm?raw";
import multiImageMix from "../../examples/textures/multi_image_mix.asm?raw";
import noiseField from "../../examples/textures/noise_field.asm?raw";
import audioScope from "../../examples/audio/audio_scope.asm?raw";
import micScope from "../../examples/microphone/mic_scope.asm?raw";
import webcamChannel from "../../examples/webcam/webcam_channel.asm?raw";
import videoChannel from "../../examples/video/video_channel.asm?raw";
import videoTexel from "../../examples/video/video_texel.asm?raw";
import videoMetadata from "../../examples/video/channel_metadata.asm?raw";
import posterEdges from "../../examples/video/poster_edges.asm?raw";

export type TemplateProject = {
  id: string;
  name: string;
  files: ProjectFile[];
  main: string;
  size?: ProjectBundle["settings"]["size"];
  scale?: number;
  channels?: ProjectBundle["settings"]["channels"];
  buffers?: ProjectBundle["settings"]["buffers"];
};

const fallbackChannels: ProjectBundle["settings"]["channels"] = [
  { kind: "fallback", name: "channel0", width: 1, height: 1 },
  { kind: "fallback", name: "channel1", width: 1, height: 1 },
  { kind: "fallback", name: "channel2", width: 1, height: 1 },
  { kind: "fallback", name: "channel3", width: 1, height: 1 }
];

const commonFiles: ProjectFile[] = [{ path: "examples/common/math.inc", content: commonMath }];

function single(path: string, content: string, extras: Partial<TemplateProject> = {}): TemplateProject {
  return {
    id: path,
    name: path.replace("examples/", "").replace(".asm", ""),
    files: [{ path, content }, ...commonFiles],
    main: path,
    size: "gba",
    scale: 4,
    ...extras
  };
}

export const templateProjects: TemplateProject[] = [
  single("examples/basics/consts.asm", consts),
  single("examples/basics/plasma.asm", plasma),
  single("examples/basics/subroutines.asm", subroutines),
  single("examples/basics/time_pulse.asm", timePulse),
  single("examples/input/live_controls.asm", liveControls),
  single("examples/input/mouse_rings.asm", mouseRings),
  {
    id: "examples/buffers/life_display.asm",
    name: "buffers/life_display",
    files: [
      { path: "examples/buffers/life_display.asm", content: lifeDisplay },
      { path: "examples/buffers/life_buffer.asm", content: lifeBuffer }
    ],
    main: "examples/buffers/life_display.asm",
    size: "gba",
    scale: 4,
    buffers: [{ file: "examples/buffers/life_buffer.asm", wgsl: "" }, null, null, null]
  },
  {
    id: "examples/buffers/ramp_display.asm",
    name: "buffers/ramp_display",
    files: [
      { path: "examples/buffers/ramp_display.asm", content: rampDisplay },
      { path: "examples/buffers/ramp_buffer.asm", content: rampBuffer }
    ],
    main: "examples/buffers/ramp_display.asm",
    size: "gba",
    scale: 4,
    buffers: [{ file: "examples/buffers/ramp_buffer.asm", wgsl: "" }, null, null, null]
  },
  {
    id: "examples/multifile/main.asm",
    name: "multifile/main",
    files: [
      { path: "examples/multifile/main.asm", content: multifileMain },
      { path: "examples/multifile/palette.inc", content: multifilePalette },
      { path: "examples/multifile/rings.inc", content: multifileRings }
    ],
    main: "examples/multifile/main.asm",
    size: "gba",
    scale: 4
  },
  single("examples/perf/heavy.asm", heavy, { size: "160x160", scale: 3 }),
  single("examples/raymarch/planet_sphere.asm", planetSphere, { size: "160x160", scale: 3 }),
  single("examples/raymarch/pixelated_planet.asm", pixelatedPlanet, { size: "506x632", scale: 1 }),
  single("examples/textures/image_passthrough.asm", imagePassthrough),
  single("examples/textures/multi_image_mix.asm", multiImageMix),
  single("examples/textures/noise_field.asm", noiseField, {
    channels: [{ kind: "noise", name: "noise:42", width: 256, height: 256, seed: "42" }, ...fallbackChannels.slice(1)]
  }),
  single("examples/audio/audio_scope.asm", audioScope, {
    channels: [{ kind: "audio", name: "choose audio file", width: 512, height: 2, sampleRate: 44100 }, ...fallbackChannels.slice(1)]
  }),
  single("examples/microphone/mic_scope.asm", micScope, {
    channels: [{ kind: "microphone", name: "microphone", width: 512, height: 2, sampleRate: 44100 }, ...fallbackChannels.slice(1)]
  }),
  single("examples/webcam/webcam_channel.asm", webcamChannel, {
    size: "320x240",
    scale: 2,
    channels: [{ kind: "webcam", name: "webcam", width: 320, height: 240 }, ...fallbackChannels.slice(1)]
  }),
  single("examples/video/video_channel.asm", videoChannel, { size: "320x180", scale: 2 }),
  single("examples/video/video_texel.asm", videoTexel, { size: "160x90", scale: 3 }),
  single("examples/video/channel_metadata.asm", videoMetadata, { size: "320x180", scale: 2 }),
  single("examples/video/poster_edges.asm", posterEdges, { size: "320x180", scale: 2 })
];

export function makeTemplateProject(template: TemplateProject): ProjectBundle {
  const files = template.files.map((file) => ({ ...file }));
  const compiled = compileAsmToWgsl(files, template.main);
  return normalizeProject({
    files,
    settings: {
      main: template.main,
      wgsl: compiled.wgsl,
      size: template.size ?? "gba",
      scale: template.scale ?? 4,
      channels: template.channels ?? fallbackChannels.map((channel) => ({ ...channel })),
      buffers: template.buffers ?? [null, null, null, null]
    }
  });
}
