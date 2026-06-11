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
  channels: ChannelSetting[];
};

export type ProjectBundle = {
  files: ProjectFile[];
  settings: ProjectSettings;
};

export type ChannelSetting = {
  name: string;
  width: number;
  height: number;
  imageDataUrl?: string;
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

norm tmp0, px, width
norm tmp1, py, height
out tmp0, tmp1, 0.5, 1.0
`;

export function makeDefaultProject(): ProjectBundle {
  const files = [{ path: "main.asm", content: defaultAsm }];
  const compiled = compileAsmToWgsl(files, "main.asm");
  return {
    files,
    settings: {
      main: "main.asm",
      wgsl: compiled.wgsl,
      size: "gba",
      scale: 4,
      channels: [
        { name: "channel0", width: 1, height: 1 },
        { name: "channel1", width: 1, height: 1 },
        { name: "channel2", width: 1, height: 1 },
        { name: "channel3", width: 1, height: 1 }
      ]
    }
  };
}

export function normalizeProject(bundle: ProjectBundle): ProjectBundle {
  const channels = [...(bundle.settings.channels ?? [])];
  while (channels.length < 4) {
    channels.push({ name: `channel${channels.length}`, width: 1, height: 1 });
  }
  return {
    ...bundle,
    settings: {
      ...bundle.settings,
      channels: channels.slice(0, 4).map((channel, index) => ({
        name: channel.name || `channel${index}`,
        width: channel.width || 1,
        height: channel.height || 1,
        imageDataUrl: channel.imageDataUrl
      }))
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
    return normalizeProject(parsed);
  } catch {
    return null;
  }
}
import { compileAsmToWgsl } from "./compiler";
