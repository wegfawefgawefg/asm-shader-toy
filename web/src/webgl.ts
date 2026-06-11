import { parseSize, type ChannelSetting, type ProjectSettings } from "./project";
import { makeNoisePixels, noiseTextureSize } from "./noise";
import type { BrowserInputState, ChannelRuntimeSource } from "./webgpu";

export type GlContext = {
  gl: WebGL2RenderingContext;
  canvas: HTMLCanvasElement;
  screenProgram: WebGLProgram;
  screenTextureUniform: WebGLUniformLocation | null;
  vao: WebGLVertexArrayObject | null;
  errors: string[];
};

export type GlProgramState = {
  source: string;
  sizeKey: string;
  program: WebGLProgram;
  outputTexture: WebGLTexture;
  framebuffer: WebGLFramebuffer;
  channelTextures: WebGLTexture[];
  channelMetadata: ChannelSetting[];
  bufferPasses: GlBufferPassState[];
  bufferReadIndex: number;
};

type GlBufferPassState = {
  channel: number;
  source: string;
  program: WebGLProgram;
  textures: [WebGLTexture, WebGLTexture];
  framebuffers: [WebGLFramebuffer, WebGLFramebuffer];
};

const vertexShaderSource = `#version 300 es
const vec2 positions[6] = vec2[6](
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);
const vec2 uvs[6] = vec2[6](
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(0.0, 0.0),
    vec2(0.0, 0.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);
out vec2 v_uv;
void main() {
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    v_uv = uvs[gl_VertexID];
}
`;

const screenFragmentShaderSource = `#version 300 es
precision highp float;
precision highp sampler2D;
in vec2 v_uv;
out vec4 fragColor;
uniform sampler2D image_texture;
void main() {
    fragColor = texture(image_texture, v_uv);
}
`;

export function initWebGl(canvas: HTMLCanvasElement): GlContext {
  const gl = canvas.getContext("webgl2", { alpha: false, antialias: false, depth: false, stencil: false });
  if (!gl) {
    throw new Error("WebGL2 is not available in this browser.");
  }
  const vao = gl.createVertexArray();
  gl.bindVertexArray(vao);
  gl.disable(gl.BLEND);
  gl.disable(gl.DEPTH_TEST);
  gl.disable(gl.STENCIL_TEST);
  const screenProgram = makeProgram(gl, vertexShaderSource, screenFragmentShaderSource, "screen");
  const screenTextureUniform = gl.getUniformLocation(screenProgram, "image_texture");
  return { gl, canvas, screenProgram, screenTextureUniform, vao, errors: [] };
}

export function configureGlCanvas(_context: GlContext): void {
  // WebGL observes canvas width/height at draw time through viewport; no explicit surface reconfigure is needed.
}

function makeShader(gl: WebGL2RenderingContext, type: number, source: string, label: string): WebGLShader {
  const shader = gl.createShader(type);
  if (!shader) {
    throw new Error(`could not create ${label} shader`);
  }
  gl.shaderSource(shader, source);
  gl.compileShader(shader);
  if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
    const info = gl.getShaderInfoLog(shader) || `unknown ${label} shader error`;
    gl.deleteShader(shader);
    throw new Error(info);
  }
  return shader;
}

function makeProgram(gl: WebGL2RenderingContext, vertexSource: string, fragmentSource: string, label: string): WebGLProgram {
  const vertex = makeShader(gl, gl.VERTEX_SHADER, vertexSource, `${label} vertex`);
  const fragment = makeShader(gl, gl.FRAGMENT_SHADER, fragmentSource, `${label} fragment`);
  const program = gl.createProgram();
  if (!program) {
    throw new Error(`could not create ${label} program`);
  }
  gl.attachShader(program, vertex);
  gl.attachShader(program, fragment);
  gl.linkProgram(program);
  gl.deleteShader(vertex);
  gl.deleteShader(fragment);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    const info = gl.getProgramInfoLog(program) || `unknown ${label} link error`;
    gl.deleteProgram(program);
    throw new Error(info);
  }
  return program;
}

function makeTexture(gl: WebGL2RenderingContext, width: number, height: number, pixels?: TexImageSource | ArrayBufferView): WebGLTexture {
  const texture = gl.createTexture();
  if (!texture) {
    throw new Error("could not create WebGL texture");
  }
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, false);
  if (pixels && ArrayBuffer.isView(pixels)) {
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
  } else if (pixels) {
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
  } else {
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, width, height, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
  }
  return texture;
}

function makeFramebuffer(gl: WebGL2RenderingContext, texture: WebGLTexture): WebGLFramebuffer {
  const framebuffer = gl.createFramebuffer();
  if (!framebuffer) {
    throw new Error("could not create WebGL framebuffer");
  }
  gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
  gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
  const status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);
  if (status !== gl.FRAMEBUFFER_COMPLETE) {
    throw new Error(`incomplete WebGL framebuffer: ${status}`);
  }
  return framebuffer;
}

function makeFallbackTexture(gl: WebGL2RenderingContext): WebGLTexture {
  return makeTexture(gl, 1, 1, new Uint8Array([255, 255, 255, 255]));
}

function makeNoiseTexture(gl: WebGL2RenderingContext, seed: string): WebGLTexture {
  return makeTexture(gl, noiseTextureSize, noiseTextureSize, makeNoisePixels(seed));
}

async function loadImageBitmap(src: string): Promise<ImageBitmap> {
  const response = await fetch(src);
  const blob = await response.blob();
  return createImageBitmap(blob);
}

async function makeChannelTexture(gl: WebGL2RenderingContext, channel: ChannelSetting): Promise<WebGLTexture> {
  if (channel.kind === "noise" || channel.seed) {
    return makeNoiseTexture(gl, channel.seed ?? channel.name);
  }
  const imageSource = channel.imageDataUrl ?? channel.sourceUrl;
  if (!imageSource) {
    return makeFallbackTexture(gl);
  }
  const image = await loadImageBitmap(imageSource);
  const texture = makeTexture(gl, image.width, image.height, image);
  image.close();
  return texture;
}

function makeLiveDataTexture(gl: WebGL2RenderingContext, width: number, height: number): WebGLTexture {
  return makeTexture(gl, Math.max(1, width), Math.max(1, height));
}

function makeOutputTexture(gl: WebGL2RenderingContext, width: number, height: number): WebGLTexture {
  return makeTexture(gl, width, height);
}

export async function createGlProgram(
  context: GlContext,
  source: string,
  settings: ProjectSettings,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource> = new Map()
): Promise<GlProgramState> {
  const { gl } = context;
  const size = parseSize(settings.size);
  const program = makeProgram(gl, vertexShaderSource, source, "image pass");
  const outputTexture = makeOutputTexture(gl, size.width, size.height);
  const framebuffer = makeFramebuffer(gl, outputTexture);
  const normalizedChannels = [...(settings.channels ?? [])];
  while (normalizedChannels.length < 4) {
    normalizedChannels.push({ kind: "fallback", name: `channel${normalizedChannels.length}`, width: 1, height: 1 });
  }
  const channelTextures = await Promise.all(
    normalizedChannels.slice(0, 4).map((channel, index) => {
      const source = channelSources.get(index);
      if (source?.audio) {
        return makeLiveDataTexture(gl, source.audio.width, source.audio.height);
      }
      if (source?.video) {
        return makeLiveDataTexture(gl, Math.max(1, source.video.videoWidth || channel.width), Math.max(1, source.video.videoHeight || channel.height));
      }
      return makeChannelTexture(gl, channel);
    })
  );
  const bufferPasses = await Promise.all(
    (settings.buffers ?? []).slice(0, 4).map(async (buffer, channel): Promise<GlBufferPassState | null> => {
      if (!buffer?.glsl) {
        return null;
      }
      const bufferProgram = makeProgram(gl, vertexShaderSource, buffer.glsl, `buffer${channel} pass`);
      const first = makeOutputTexture(gl, size.width, size.height);
      const second = makeOutputTexture(gl, size.width, size.height);
      return {
        channel,
        source: buffer.glsl,
        program: bufferProgram,
        textures: [first, second],
        framebuffers: [makeFramebuffer(gl, first), makeFramebuffer(gl, second)]
      };
    })
  );
  return {
    source,
    sizeKey: `${size.width}x${size.height}`,
    program,
    outputTexture,
    framebuffer,
    channelTextures,
    channelMetadata: normalizedChannels.slice(0, 4),
    bufferPasses: bufferPasses.filter((buffer): buffer is GlBufferPassState => buffer !== null),
    bufferReadIndex: 0
  };
}

export function destroyGlProgram(context: GlContext, program: GlProgramState | null): void {
  if (!program) {
    return;
  }
  const { gl } = context;
  gl.deleteProgram(program.program);
  gl.deleteTexture(program.outputTexture);
  gl.deleteFramebuffer(program.framebuffer);
  for (const texture of program.channelTextures) {
    gl.deleteTexture(texture);
  }
  for (const buffer of program.bufferPasses) {
    gl.deleteProgram(buffer.program);
    gl.deleteTexture(buffer.textures[0]);
    gl.deleteTexture(buffer.textures[1]);
    gl.deleteFramebuffer(buffer.framebuffers[0]);
    gl.deleteFramebuffer(buffer.framebuffers[1]);
  }
}

function updateLiveChannelTextures(
  context: GlContext,
  program: GlProgramState,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource>
): void {
  const { gl } = context;
  for (let channel = 0; channel < program.channelTextures.length; ++channel) {
    const source = channelSources.get(channel);
    gl.bindTexture(gl.TEXTURE_2D, program.channelTextures[channel]);
    if (source?.audio) {
      const { analyser, timeData, frequencyData, pixels, width, height } = source.audio;
      analyser.getByteTimeDomainData(timeData);
      analyser.getByteFrequencyData(frequencyData);
      let offset = 0;
      for (let x = 0; x < width; ++x) {
        const value = timeData[x] ?? 128;
        pixels[offset++] = value;
        pixels[offset++] = value;
        pixels[offset++] = value;
        pixels[offset++] = 255;
      }
      for (let x = 0; x < width; ++x) {
        const value = frequencyData[x] ?? 0;
        pixels[offset++] = value;
        pixels[offset++] = value;
        pixels[offset++] = value;
        pixels[offset++] = 255;
      }
      gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
      continue;
    }
    const video = source?.video;
    if (!source || !video || video.readyState < HTMLMediaElement.HAVE_CURRENT_DATA) {
      continue;
    }
    const width = Math.max(1, video.videoWidth);
    const height = Math.max(1, video.videoHeight);
    if (source.mirrored) {
      if (!source.mirrorCanvas || !source.mirrorContext) {
        source.mirrorCanvas = document.createElement("canvas");
        source.mirrorContext = source.mirrorCanvas.getContext("2d") ?? undefined;
      }
      if (!source.mirrorContext) {
        continue;
      }
      source.mirrorCanvas.width = width;
      source.mirrorCanvas.height = height;
      source.mirrorContext.setTransform(-1, 0, 0, 1, width, 0);
      source.mirrorContext.drawImage(video, 0, 0, width, height);
      source.mirrorContext.setTransform(1, 0, 0, 1, 0, 0);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, source.mirrorCanvas);
      continue;
    }
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, video);
  }
}

function bindTexture(gl: WebGL2RenderingContext, program: WebGLProgram, name: string, unit: number, texture: WebGLTexture): void {
  gl.activeTexture(gl.TEXTURE0 + unit);
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.uniform1i(gl.getUniformLocation(program, name), unit);
}

function setUniforms(
  context: GlContext,
  program: WebGLProgram,
  settings: ProjectSettings,
  frame: number,
  start: number,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource>,
  inputState?: BrowserInputState
): void {
  const { gl } = context;
  const size = parseSize(settings.size);
  const now = performance.now() / 1000;
  const set1 = (name: string, value: number): void => gl.uniform1f(gl.getUniformLocation(program, name), value);
  set1("ast_time", now - start);
  set1("ast_time_delta", 1 / 60);
  set1("ast_frame", frame);
  set1("ast_width", size.width);
  set1("ast_height", size.height);
  set1("ast_mouse_x", inputState?.mouseDown ? inputState.mouseX : 0);
  set1("ast_mouse_y", inputState?.mouseDown ? inputState.mouseY : 0);
  set1("ast_mouse_down", inputState?.mouseDown ?? 0);
  set1("ast_mouse_click_x", inputState?.mouseClickX ?? 0);
  set1("ast_mouse_click_y", inputState?.mouseClickY ?? 0);
  const wallClock = new Date();
  set1("ast_wall_clock_seconds", wallClock.getHours() * 3600 + wallClock.getMinutes() * 60 + wallClock.getSeconds() + wallClock.getMilliseconds() / 1000);
  set1("ast_year", wallClock.getFullYear());
  set1("ast_month", wallClock.getMonth() + 1);
  set1("ast_day", wallClock.getDate());
  for (let channel = 0; channel < 4; ++channel) {
    const metadata = settings.channels[channel] ?? { width: 1, height: 1, name: `channel${channel}` };
    const source = channelSources.get(channel);
    const buffer = settings.buffers?.[channel];
    const width = buffer?.wgsl ? size.width : metadata.width || 1;
    const height = buffer?.wgsl ? size.height : metadata.height || 1;
    const time =
      metadata.kind === "video" && source?.video
        ? source.video.currentTime
        : metadata.kind === "audio" && source?.audio?.startedAt !== undefined
          ? source.audio.duration
            ? (now - source.audio.startedAt) % source.audio.duration
            : now - source.audio.startedAt
          : metadata.kind === "webcam" || metadata.kind === "microphone"
            ? now - start
            : 0;
    gl.uniform4f(gl.getUniformLocation(program, `ast_channel${channel}`), width, height, time, metadata.sampleRate ?? 0);
  }
  const mouseButtons = new Float32Array(8);
  for (let index = 0; index < Math.min(8, inputState?.mouseButtons.length ?? 0); ++index) {
    mouseButtons[index] = inputState?.mouseButtons[index] ?? 0;
  }
  gl.uniform4fv(gl.getUniformLocation(program, "ast_mouse_buttons[0]"), mouseButtons);
  gl.uniform4f(gl.getUniformLocation(program, "ast_mouse_wheel"), inputState?.mouseWheelX ?? 0, inputState?.mouseWheelY ?? 0, 0, 0);
}

function drawPass(
  context: GlContext,
  program: WebGLProgram,
  framebuffer: WebGLFramebuffer,
  targetWidth: number,
  targetHeight: number,
  channelTextures: WebGLTexture[],
  settings: ProjectSettings,
  frame: number,
  start: number,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource>,
  inputState?: BrowserInputState
): void {
  const { gl } = context;
  gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
  gl.viewport(0, 0, targetWidth, targetHeight);
  gl.useProgram(program);
  setUniforms(context, program, settings, frame, start, channelSources, inputState);
  for (let index = 0; index < 4; ++index) {
    bindTexture(gl, program, `channel${index}_texture`, index, channelTextures[index]);
  }
  gl.drawArrays(gl.TRIANGLES, 0, 6);
}

export function renderGlFrame(
  context: GlContext,
  program: GlProgramState,
  settings: ProjectSettings,
  frame: number,
  start: number,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource> = new Map(),
  inputState?: BrowserInputState
): void {
  const { gl } = context;
  const size = parseSize(settings.size);
  updateLiveChannelTextures(context, program, channelSources);
  const readIndex = program.bufferReadIndex;
  const writeIndex = 1 - readIndex;
  const previousBufferTextures = new Map(program.bufferPasses.map((buffer) => [buffer.channel, buffer.textures[readIndex]]));
  const currentBufferTextures = new Map(program.bufferPasses.map((buffer) => [buffer.channel, buffer.textures[writeIndex]]));
  const previousChannelTextures = program.channelTextures.map((texture, channel) => previousBufferTextures.get(channel) ?? texture);
  const currentChannelTextures = program.channelTextures.map((texture, channel) => currentBufferTextures.get(channel) ?? texture);

  for (const buffer of program.bufferPasses) {
    drawPass(context, buffer.program, buffer.framebuffers[writeIndex], size.width, size.height, previousChannelTextures, settings, frame, start, channelSources, inputState);
  }
  drawPass(context, program.program, program.framebuffer, size.width, size.height, currentChannelTextures, settings, frame, start, channelSources, inputState);

  gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  gl.viewport(0, 0, context.canvas.width, context.canvas.height);
  gl.useProgram(context.screenProgram);
  bindTexture(gl, context.screenProgram, "image_texture", 0, program.outputTexture);
  gl.drawArrays(gl.TRIANGLES, 0, 6);
  const error = gl.getError();
  if (error !== gl.NO_ERROR) {
    context.errors.push(`WebGL error: ${error}`);
  }
  if (program.bufferPasses.length > 0) {
    program.bufferReadIndex = writeIndex;
  }
}
