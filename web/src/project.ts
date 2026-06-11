export type ProjectFile = {
  path: string;
  content: string;
};

export type SizePreset = "gb" | "gba" | "nes" | "snes" | "n64" | "ps1" | "psp";

export type ProjectSettings = {
  main: string;
  wgsl: string;
  size: SizePreset | `${number}x${number}`;
  scale: number;
};

export type ProjectBundle = {
  files: ProjectFile[];
  settings: ProjectSettings;
};

export const sizePresets: Record<SizePreset, { width: number; height: number }> = {
  gb: { width: 160, height: 144 },
  gba: { width: 240, height: 160 },
  nes: { width: 256, height: 240 },
  snes: { width: 256, height: 224 },
  n64: { width: 320, height: 240 },
  ps1: { width: 320, height: 240 },
  psp: { width: 480, height: 272 }
};

export function parseSize(value: ProjectSettings["size"]): { width: number; height: number } {
  if (value in sizePresets) {
    return sizePresets[value as SizePreset];
  }
  const match = /^([1-9]\d*)x([1-9]\d*)$/.exec(value);
  if (!match) {
    return sizePresets.gba;
  }
  return { width: Number(match[1]), height: Number(match[2]) };
}

const defaultAsm = `.include <std/screen.inc>

; Browser compiler parity is not wired yet.
; For now, use the native CLI to emit WGSL:
; ./build/asm-shader-toy examples/basics/time_pulse.asm --emit-wgsl -

norm tmp0, px, width
norm tmp1, py, height
out tmp0, tmp1, 0.5, 1.0
`;

const defaultWgsl = `struct AstInputs {
    time: f32,
    time_delta: f32,
    frame: f32,
    width: f32,
    height: f32,
    mouse_x: f32,
    mouse_y: f32,
    mouse_down: f32,
    mouse_click_x: f32,
    mouse_click_y: f32,
    wall_clock_seconds: f32,
    year: f32,
    month: f32,
    day: f32,
    channel0: vec4<f32>,
    channel1: vec4<f32>,
    channel2: vec4<f32>,
    channel3: vec4<f32>,
    keys: array<vec4<f32>, 128>,
    mouse_buttons: array<vec4<f32>, 2>,
    mouse_wheel: vec4<f32>,
    gamepad_buttons: array<vec4<f32>, 8>,
    gamepad_axes: array<vec4<f32>, 4>,
};

@group(0) @binding(0) var output_texture: texture_storage_2d<rgba8unorm, write>;
@group(0) @binding(1) var<uniform> ast_inputs: AstInputs;
@group(0) @binding(2) var channel0_texture: texture_2d<f32>;
@group(0) @binding(3) var channel1_texture: texture_2d<f32>;
@group(0) @binding(4) var channel2_texture: texture_2d<f32>;
@group(0) @binding(5) var channel3_texture: texture_2d<f32>;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    if (gid.x >= u32(ast_inputs.width) || gid.y >= u32(ast_inputs.height)) {
        return;
    }
    let uv = vec2<f32>(f32(gid.x) / ast_inputs.width, f32(gid.y) / ast_inputs.height);
    let pulse = 0.5 + 0.5 * sin(ast_inputs.time * 2.0);
    let color = vec4<f32>(uv.x, uv.y, pulse, 1.0);
    textureStore(output_texture, vec2<i32>(i32(gid.x), i32(gid.y)), color);
}
`;

export function makeDefaultProject(): ProjectBundle {
  return {
    files: [{ path: "main.asm", content: defaultAsm }],
    settings: {
      main: "main.asm",
      wgsl: defaultWgsl,
      size: "gba",
      scale: 4
    }
  };
}

function base64UrlEncode(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) {
    binary += String.fromCharCode(byte);
  }
  return btoa(binary).replaceAll("+", "-").replaceAll("/", "_").replaceAll("=", "");
}

function base64UrlDecode(text: string): Uint8Array {
  const padded = text.replaceAll("-", "+").replaceAll("_", "/").padEnd(Math.ceil(text.length / 4) * 4, "=");
  const binary = atob(padded);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; ++i) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

async function gzip(bytes: Uint8Array): Promise<Uint8Array> {
  if (!("CompressionStream" in globalThis)) {
    return bytes;
  }
  const input = new Uint8Array(bytes);
  const stream = new CompressionStream("gzip");
  const writer = stream.writable.getWriter();
  void writer.write(input);
  void writer.close();
  return new Uint8Array(await new Response(stream.readable).arrayBuffer());
}

async function gunzip(bytes: Uint8Array): Promise<Uint8Array> {
  if (!("DecompressionStream" in globalThis)) {
    return bytes;
  }
  const input = new Uint8Array(bytes);
  const stream = new DecompressionStream("gzip");
  const writer = stream.writable.getWriter();
  void writer.write(input);
  void writer.close();
  return new Uint8Array(await new Response(stream.readable).arrayBuffer());
}

export async function encodeProject(bundle: ProjectBundle): Promise<string> {
  const json = JSON.stringify(bundle);
  const compressed = await gzip(new TextEncoder().encode(json));
  return `#project=${base64UrlEncode(compressed)}`;
}

export async function decodeProject(hash: string): Promise<ProjectBundle | null> {
  const params = new URLSearchParams(hash.startsWith("#") ? hash.slice(1) : hash);
  const encoded = params.get("project");
  if (!encoded) {
    return null;
  }
  try {
    const bytes = await gunzip(base64UrlDecode(encoded));
    const parsed = JSON.parse(new TextDecoder().decode(bytes)) as ProjectBundle;
    if (!Array.isArray(parsed.files) || parsed.files.length === 0 || !parsed.settings) {
      return null;
    }
    return parsed;
  } catch {
    return null;
  }
}
