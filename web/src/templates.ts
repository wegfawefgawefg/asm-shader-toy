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
  maxSteps?: number;
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

const checkerChannel: ProjectBundle["settings"]["channels"][number] = {
  kind: "image",
  name: "checker.png",
  width: 8,
  height: 8,
  sourceUrl: "examples/assets/checker.png"
};

const barsChannel: ProjectBundle["settings"]["channels"][number] = {
  kind: "image",
  name: "bars.png",
  width: 8,
  height: 8,
  sourceUrl: "examples/assets/bars.png"
};

const twoToneChannel: ProjectBundle["settings"]["channels"][number] = {
  kind: "audio",
  name: "two_tone.wav",
  width: 512,
  height: 2,
  sampleRate: 44100,
  sourceUrl: "examples/assets/audio/two_tone.wav"
};

const testsrcVideoChannel: ProjectBundle["settings"]["channels"][number] = {
  kind: "video",
  name: "testsrc_160x90.mp4",
  width: 160,
  height: 90,
  sourceUrl: "examples/assets/video/testsrc_160x90.mp4"
};

const bigBuckBunnyChannel: ProjectBundle["settings"]["channels"][number] = {
  kind: "video",
  name: "big_buck_bunny_4m34s_640x360.mp4",
  width: 640,
  height: 360,
  sourceUrl: "examples/assets/video/big_buck_bunny_4m34s_640x360.mp4"
};

function withChannels(
  channels: Array<ProjectBundle["settings"]["channels"][number]>
): ProjectBundle["settings"]["channels"] {
  return [...channels.map((channel) => ({ ...channel })), ...fallbackChannels.slice(channels.length).map((channel) => ({ ...channel }))];
}

function single(path: string, content: string, extras: Partial<TemplateProject> = {}): TemplateProject {
  return {
    id: path,
    name: path.replace("examples/", "").replace(".asm", ""),
    files: [{ path, content }, ...commonFiles],
    main: path,
    size: "gb",
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
  single("examples/raymarch/pixelated_planet.asm", pixelatedPlanet, { maxSteps: 16384 }),
  single("examples/textures/image_passthrough.asm", imagePassthrough, { channels: withChannels([checkerChannel]) }),
  single("examples/textures/multi_image_mix.asm", multiImageMix, { channels: withChannels([checkerChannel, barsChannel]) }),
  single("examples/textures/noise_field.asm", noiseField, {
    channels: [{ kind: "noise", name: "noise:42", width: 256, height: 256, seed: "42" }, ...fallbackChannels.slice(1)]
  }),
  single("examples/audio/audio_scope.asm", audioScope, { channels: withChannels([twoToneChannel]) }),
  single("examples/microphone/mic_scope.asm", micScope, {
    channels: [{ kind: "microphone", name: "microphone", width: 512, height: 2, sampleRate: 44100 }, ...fallbackChannels.slice(1)]
  }),
  single("examples/webcam/webcam_channel.asm", webcamChannel, {
    size: "320x240",
    scale: 2,
    channels: [{ kind: "webcam", name: "webcam", width: 320, height: 240 }, ...fallbackChannels.slice(1)]
  }),
  single("examples/video/video_channel.asm", videoChannel, { size: "320x180", scale: 2, channels: withChannels([bigBuckBunnyChannel]) }),
  single("examples/video/video_texel.asm", videoTexel, { size: "160x90", scale: 3, channels: withChannels([testsrcVideoChannel]) }),
  single("examples/video/channel_metadata.asm", videoMetadata, { size: "320x180", scale: 2, channels: withChannels([bigBuckBunnyChannel]) }),
  single("examples/video/poster_edges.asm", posterEdges, { size: "320x180", scale: 2, channels: withChannels([bigBuckBunnyChannel]) })
];

export function makeTemplateProject(template: TemplateProject): ProjectBundle {
  const files = template.files.map((file) => ({ ...file }));
  const compiled = compileAsmToWgsl(files, template.main, template.maxSteps ?? 4096);
  return normalizeProject({
    files,
    settings: {
      main: template.main,
      wgsl: compiled.wgsl,
      size: template.size ?? "gba",
      scale: template.scale ?? 4,
      maxSteps: template.maxSteps ?? 4096,
      channels: template.channels ?? fallbackChannels.map((channel) => ({ ...channel })),
      buffers: template.buffers ?? [null, null, null, null]
    }
  });
}
