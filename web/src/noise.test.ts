import { describe, expect, test } from "vitest";
import { makeNoisePixels } from "./noise";

describe("generated noise channels", () => {
  test("creates deterministic rgba pixels", () => {
    const first = makeNoisePixels("clouds", 4);
    const second = makeNoisePixels("clouds", 4);

    expect(first).toEqual(second);
    expect(first).toHaveLength(4 * 4 * 4);
    for (let offset = 3; offset < first.length; offset += 4) {
      expect(first[offset]).toBe(255);
    }
  });

  test("changes when the seed changes", () => {
    expect(makeNoisePixels("clouds", 4)).not.toEqual(makeNoisePixels("marble", 4));
  });
});
