import { makeNoisePixels, noiseTextureSize } from "./noise";
import { parseSize, type ChannelSetting, type ProjectSettings } from "./project";

type GpuContext = {
  device: GPUDevice;
  canvasContext: GPUCanvasContext;
  format: GPUTextureFormat;
  sampler: GPUSampler;
  renderPipeline: GPURenderPipeline;
};

type ProgramState = {
  source: string;
  sizeKey: string;
  computePipeline: GPUComputePipeline;
  uniformBuffer: GPUBuffer;
  outputTexture: GPUTexture;
  outputView: GPUTextureView;
  channelTextures: GPUTexture[];
  channelMetadata: ChannelSetting[];
  bufferPasses: BufferPassState[];
  bufferReadIndex: number;
};

type BufferPassState = {
  channel: number;
  source: string;
  computePipeline: GPUComputePipeline;
  textures: [GPUTexture, GPUTexture];
  views: [GPUTextureView, GPUTextureView];
};

export type ChannelRuntimeSource = {
  audio?: {
    analyser: AnalyserNode;
    timeData: Uint8Array<ArrayBuffer>;
    frequencyData: Uint8Array<ArrayBuffer>;
    pixels: Uint8Array<ArrayBuffer>;
    width: number;
    height: number;
  };
  video?: HTMLVideoElement;
  mirrorCanvas?: HTMLCanvasElement;
  mirrorContext?: CanvasRenderingContext2D;
  mirrored?: boolean;
};

const renderShader = `
struct VertexOut {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs(@builtin(vertex_index) vertex_index: u32) -> VertexOut {
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0)
    );
    var uvs = array<vec2<f32>, 3>(
        vec2<f32>(0.0, 1.0),
        vec2<f32>(2.0, 1.0),
        vec2<f32>(0.0, -1.0)
    );
    var out: VertexOut;
    out.position = vec4<f32>(positions[vertex_index], 0.0, 1.0);
    out.uv = uvs[vertex_index];
    return out;
}

@group(0) @binding(0) var image_sampler: sampler;
@group(0) @binding(1) var image_texture: texture_2d<f32>;

@fragment
fn fs(in: VertexOut) -> @location(0) vec4<f32> {
    return textureSample(image_texture, image_sampler, in.uv);
}
`;

const uniformFloatCount = 14 + 16 + 512 + 8 + 4 + 32 + 16;
const uniformByteSize = uniformFloatCount * 4;

export async function initWebGpu(canvas: HTMLCanvasElement): Promise<GpuContext> {
  if (!navigator.gpu) {
    throw new Error("WebGPU is not available in this browser.");
  }
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) {
    throw new Error("No WebGPU adapter is available.");
  }
  const device = await adapter.requestDevice();
  const canvasContext = canvas.getContext("webgpu");
  if (!canvasContext) {
    throw new Error("Could not create WebGPU canvas context.");
  }
  const format = navigator.gpu.getPreferredCanvasFormat();
  canvasContext.configure({ device, format, alphaMode: "opaque" });

  const sampler = device.createSampler({
    magFilter: "nearest",
    minFilter: "nearest",
    mipmapFilter: "nearest"
  });

  const renderModule = device.createShaderModule({ code: renderShader });
  const renderPipeline = device.createRenderPipeline({
    layout: "auto",
    vertex: { module: renderModule, entryPoint: "vs" },
    fragment: { module: renderModule, entryPoint: "fs", targets: [{ format }] },
    primitive: { topology: "triangle-list" }
  });

  return { device, canvasContext, format, sampler, renderPipeline };
}

function makeFallbackTexture(device: GPUDevice): GPUTexture {
  const texture = device.createTexture({
    size: { width: 1, height: 1 },
    format: "rgba8unorm",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST
  });
  device.queue.writeTexture({ texture }, new Uint8Array([255, 255, 255, 255]), {}, { width: 1, height: 1 });
  return texture;
}

function makeNoiseTexture(device: GPUDevice, seed: string): GPUTexture {
  const texture = device.createTexture({
    size: { width: noiseTextureSize, height: noiseTextureSize },
    format: "rgba8unorm",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST
  });
  device.queue.writeTexture(
    { texture },
    makeNoisePixels(seed),
    { bytesPerRow: noiseTextureSize * 4, rowsPerImage: noiseTextureSize },
    { width: noiseTextureSize, height: noiseTextureSize }
  );
  return texture;
}

function writeUniforms(
  device: GPUDevice,
  buffer: GPUBuffer,
  settings: ProjectSettings,
  frame: number,
  start: number,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource>
): void {
  const size = parseSize(settings.size);
  const values = new Float32Array(uniformFloatCount);
  const now = performance.now() / 1000;
  values[0] = now - start;
  values[1] = 1 / 60;
  values[2] = frame;
  values[3] = size.width;
  values[4] = size.height;
  values[13] = 1;
  for (let channel = 0; channel < 4; ++channel) {
    const metadata = settings.channels[channel] ?? { width: 1, height: 1 };
    const source = channelSources.get(channel);
    const buffer = settings.buffers?.[channel];
    const offset = 14 + channel * 4;
    values[offset] = buffer?.wgsl ? size.width : metadata.width || 1;
    values[offset + 1] = buffer?.wgsl ? size.height : metadata.height || 1;
    values[offset + 2] =
      metadata.kind === "video" && source?.video
        ? source.video.currentTime
        : metadata.kind === "webcam" || metadata.kind === "microphone"
          ? now - start
          : 0;
    values[offset + 3] = metadata.sampleRate ?? 0;
  }
  device.queue.writeBuffer(buffer, 0, values);
}

async function loadImageBitmap(dataUrl: string): Promise<ImageBitmap> {
  const response = await fetch(dataUrl);
  const blob = await response.blob();
  return createImageBitmap(blob);
}

async function makeChannelTexture(device: GPUDevice, channel: ChannelSetting): Promise<GPUTexture> {
  if (channel.kind === "noise" || channel.seed) {
    return makeNoiseTexture(device, channel.seed ?? channel.name);
  }
  if (!channel.imageDataUrl) {
    return makeFallbackTexture(device);
  }
  const image = await loadImageBitmap(channel.imageDataUrl);
  const texture = device.createTexture({
    size: { width: image.width, height: image.height },
    format: "rgba8unorm",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT
  });
  device.queue.copyExternalImageToTexture({ source: image }, { texture }, { width: image.width, height: image.height });
  image.close();
  return texture;
}

function makeVideoTexture(device: GPUDevice, video: HTMLVideoElement): GPUTexture {
  const width = Math.max(1, video.videoWidth);
  const height = Math.max(1, video.videoHeight);
  return device.createTexture({
    size: { width, height },
    format: "rgba8unorm",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT
  });
}

function makeLiveDataTexture(device: GPUDevice, width: number, height: number): GPUTexture {
  return device.createTexture({
    size: { width: Math.max(1, width), height: Math.max(1, height) },
    format: "rgba8unorm",
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST
  });
}

function makeOutputTexture(device: GPUDevice, width: number, height: number): GPUTexture {
  return device.createTexture({
    size: { width, height },
    format: "rgba8unorm",
    usage:
      GPUTextureUsage.STORAGE_BINDING |
      GPUTextureUsage.TEXTURE_BINDING |
      GPUTextureUsage.COPY_SRC |
      GPUTextureUsage.RENDER_ATTACHMENT
  });
}

export async function createProgram(
  context: GpuContext,
  source: string,
  settings: ProjectSettings,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource> = new Map()
): Promise<ProgramState> {
  const { device } = context;
  const size = parseSize(settings.size);
  const module = device.createShaderModule({ code: source });
  const computePipeline = await device.createComputePipelineAsync({
    layout: "auto",
    compute: { module, entryPoint: "main" }
  });
  const outputTexture = makeOutputTexture(device, size.width, size.height);
  const outputView = outputTexture.createView();
  const uniformBuffer = device.createBuffer({
    size: uniformByteSize,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });
  const normalizedChannels = [...(settings.channels ?? [])];
  while (normalizedChannels.length < 4) {
    normalizedChannels.push({ kind: "fallback", name: `channel${normalizedChannels.length}`, width: 1, height: 1 });
  }
  const channelTextures = await Promise.all(
    normalizedChannels.slice(0, 4).map((channel, index) => {
      const source = channelSources.get(index);
      const audio = source?.audio;
      if (audio) {
        return makeLiveDataTexture(device, audio.width, audio.height);
      }
      const video = source?.video;
      if (video) {
        return makeVideoTexture(device, video);
      }
      return makeChannelTexture(device, channel);
    })
  );
  const bufferPasses = await Promise.all(
    (settings.buffers ?? []).slice(0, 4).map(async (buffer, channel): Promise<BufferPassState | null> => {
      if (!buffer?.wgsl) {
        return null;
      }
      const bufferModule = device.createShaderModule({ code: buffer.wgsl });
      const bufferPipeline = await device.createComputePipelineAsync({
        layout: "auto",
        compute: { module: bufferModule, entryPoint: "main" }
      });
      const first = makeOutputTexture(device, size.width, size.height);
      const second = makeOutputTexture(device, size.width, size.height);
      return {
        channel,
        source: buffer.wgsl,
        computePipeline: bufferPipeline,
        textures: [first, second],
        views: [first.createView(), second.createView()]
      };
    })
  );
  return {
    source,
    sizeKey: `${size.width}x${size.height}`,
    computePipeline,
    uniformBuffer,
    outputTexture,
    outputView,
    channelTextures,
    channelMetadata: normalizedChannels.slice(0, 4),
    bufferPasses: bufferPasses.filter((buffer): buffer is BufferPassState => buffer !== null),
    bufferReadIndex: 0
  };
}

export function destroyProgram(program: ProgramState | null): void {
  if (!program) {
    return;
  }
  program.outputTexture.destroy();
  program.uniformBuffer.destroy();
  for (const texture of program.channelTextures) {
    texture.destroy();
  }
  for (const buffer of program.bufferPasses) {
    buffer.textures[0].destroy();
    buffer.textures[1].destroy();
  }
}

function updateLiveChannelTextures(
  context: GpuContext,
  program: ProgramState,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource>
): void {
  for (let channel = 0; channel < program.channelTextures.length; ++channel) {
    const source = channelSources.get(channel);
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
      context.device.queue.writeTexture(
        { texture: program.channelTextures[channel] },
        pixels,
        { bytesPerRow: width * 4, rowsPerImage: height },
        { width, height }
      );
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
      context.device.queue.copyExternalImageToTexture(
        { source: source.mirrorCanvas },
        { texture: program.channelTextures[channel] },
        { width, height }
      );
      continue;
    }
    context.device.queue.copyExternalImageToTexture(
      { source: video },
      { texture: program.channelTextures[channel] },
      { width, height }
    );
  }
}

export function renderFrame(
  context: GpuContext,
  program: ProgramState,
  settings: ProjectSettings,
  frame: number,
  start: number,
  channelSources: ReadonlyMap<number, ChannelRuntimeSource> = new Map()
): void {
  const { device } = context;
  const size = parseSize(settings.size);
  updateLiveChannelTextures(context, program, channelSources);
  writeUniforms(device, program.uniformBuffer, settings, frame, start, channelSources);

  const baseChannelViews = program.channelTextures.map((texture) => texture.createView());
  const readIndex = program.bufferReadIndex;
  const writeIndex = 1 - readIndex;
  const previousBufferViews = new Map(program.bufferPasses.map((buffer) => [buffer.channel, buffer.views[readIndex]]));
  const currentBufferViews = new Map(program.bufferPasses.map((buffer) => [buffer.channel, buffer.views[writeIndex]]));
  const previousChannelViews = baseChannelViews.map((view, channel) => previousBufferViews.get(channel) ?? view);
  const currentChannelViews = baseChannelViews.map((view, channel) => currentBufferViews.get(channel) ?? view);

  const renderBindGroup = device.createBindGroup({
    layout: context.renderPipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: context.sampler },
      { binding: 1, resource: program.outputView }
    ]
  });
  const encoder = device.createCommandEncoder();
  for (const buffer of program.bufferPasses) {
    const bindGroup = device.createBindGroup({
      layout: buffer.computePipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: buffer.views[writeIndex] },
        { binding: 1, resource: { buffer: program.uniformBuffer } },
        ...previousChannelViews.map((view, index) => ({ binding: index + 2, resource: view }))
      ]
    });
    const pass = encoder.beginComputePass();
    pass.setPipeline(buffer.computePipeline);
    pass.setBindGroup(0, bindGroup);
    pass.dispatchWorkgroups(Math.ceil(size.width / 8), Math.ceil(size.height / 8));
    pass.end();
  }
  const imageBindGroup = device.createBindGroup({
    layout: program.computePipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: program.outputView },
      { binding: 1, resource: { buffer: program.uniformBuffer } },
      ...currentChannelViews.map((view, index) => ({ binding: index + 2, resource: view }))
    ]
  });
  const computePass = encoder.beginComputePass();
  computePass.setPipeline(program.computePipeline);
  computePass.setBindGroup(0, imageBindGroup);
  computePass.dispatchWorkgroups(Math.ceil(size.width / 8), Math.ceil(size.height / 8));
  computePass.end();

  const renderPass = encoder.beginRenderPass({
    colorAttachments: [
      {
        view: context.canvasContext.getCurrentTexture().createView(),
        clearValue: { r: 0, g: 0, b: 0, a: 1 },
        loadOp: "clear",
        storeOp: "store"
      }
    ]
  });
  renderPass.setPipeline(context.renderPipeline);
  renderPass.setBindGroup(0, renderBindGroup);
  renderPass.draw(3);
  renderPass.end();

  device.queue.submit([encoder.finish()]);
  if (program.bufferPasses.length > 0) {
    program.bufferReadIndex = writeIndex;
  }
}
