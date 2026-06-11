export const noiseTextureSize = 256;

function seedFromText(text: string): number {
  let hash = 2166136261;
  for (let index = 0; index < text.length; ++index) {
    hash ^= text.charCodeAt(index);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

function hashU32(value: number): number {
  value >>>= 0;
  value ^= value >>> 16;
  value = Math.imul(value, 0x7feb352d);
  value ^= value >>> 15;
  value = Math.imul(value, 0x846ca68b);
  value ^= value >>> 16;
  return value >>> 0;
}

export function makeNoisePixels(seedText: string, size = noiseTextureSize): Uint8Array<ArrayBuffer> {
  const pixels = new Uint8Array(new ArrayBuffer(size * size * 4));
  const seed = seedFromText(seedText || "noise");
  let offset = 0;
  for (let y = 0; y < size; ++y) {
    for (let x = 0; x < size; ++x) {
      const base = hashU32(seed ^ Math.imul(x + 1, 374761393) ^ Math.imul(y + 1, 668265263));
      pixels[offset++] = hashU32(base ^ 0x8da6b343) & 0xff;
      pixels[offset++] = hashU32(base ^ 0xd8163841) & 0xff;
      pixels[offset++] = hashU32(base ^ 0xcb1ab31f) & 0xff;
      pixels[offset++] = 255;
    }
  }
  return pixels;
}
