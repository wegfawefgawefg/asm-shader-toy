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
  bindGroup: GPUBindGroup;
  uniformBuffer: GPUBuffer;
  outputTexture: GPUTexture;
  outputView: GPUTextureView;
  channelTextures: GPUTexture[];
  channelMetadata: ChannelSetting[];
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

function writeUniforms(device: GPUDevice, buffer: GPUBuffer, settings: ProjectSettings, frame: number, start: number): void {
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
    const offset = 14 + channel * 4;
    values[offset] = metadata.width || 1;
    values[offset + 1] = metadata.height || 1;
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

export async function createProgram(context: GpuContext, source: string, settings: ProjectSettings): Promise<ProgramState> {
  const { device } = context;
  const size = parseSize(settings.size);
  const module = device.createShaderModule({ code: source });
  const computePipeline = await device.createComputePipelineAsync({
    layout: "auto",
    compute: { module, entryPoint: "main" }
  });
  const outputTexture = device.createTexture({
    size: { width: size.width, height: size.height },
    format: "rgba8unorm",
    usage:
      GPUTextureUsage.STORAGE_BINDING |
      GPUTextureUsage.TEXTURE_BINDING |
      GPUTextureUsage.COPY_SRC |
      GPUTextureUsage.RENDER_ATTACHMENT
  });
  const outputView = outputTexture.createView();
  const uniformBuffer = device.createBuffer({
    size: uniformByteSize,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });
  const normalizedChannels = [...(settings.channels ?? [])];
  while (normalizedChannels.length < 4) {
    normalizedChannels.push({ kind: "fallback", name: `channel${normalizedChannels.length}`, width: 1, height: 1 });
  }
  const channelTextures = await Promise.all(normalizedChannels.slice(0, 4).map((channel) => makeChannelTexture(device, channel)));
  const bindGroup = device.createBindGroup({
    layout: computePipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: outputView },
      { binding: 1, resource: { buffer: uniformBuffer } },
      ...channelTextures.map((texture, index) => ({ binding: index + 2, resource: texture.createView() }))
    ]
  });
  return {
    source,
    sizeKey: `${size.width}x${size.height}`,
    computePipeline,
    bindGroup,
    uniformBuffer,
    outputTexture,
    outputView,
    channelTextures,
    channelMetadata: normalizedChannels.slice(0, 4)
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
}

export function renderFrame(context: GpuContext, program: ProgramState, settings: ProjectSettings, frame: number, start: number): void {
  const { device } = context;
  const size = parseSize(settings.size);
  writeUniforms(device, program.uniformBuffer, settings, frame, start);

  const renderBindGroup = device.createBindGroup({
    layout: context.renderPipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: context.sampler },
      { binding: 1, resource: program.outputView }
    ]
  });
  const encoder = device.createCommandEncoder();
  const computePass = encoder.beginComputePass();
  computePass.setPipeline(program.computePipeline);
  computePass.setBindGroup(0, program.bindGroup);
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
}
