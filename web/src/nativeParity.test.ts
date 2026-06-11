import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { describe, expect, test } from "vitest";
import { compileAsmToWgsl, type CompileResult } from "./compiler";

type Fixture = {
  name: string;
  main: string;
  files?: Record<string, string>;
  tokens: string[];
};

const nativeCli = process.env.AST_NATIVE_CLI ?? resolve(import.meta.dirname, "..", "..", "build", "asm-shader-toy");
const parityDescribe = process.env.AST_NATIVE_CLI ? describe : describe.skip;

const fixtures: Fixture[] = [
  {
    name: "arithmetic control flow calls and output",
    main: `.include <std/screen.inc>
mov r48, 0
loop:
add r48, r48, 1
jlt r48, 3, loop
call tint
out uv_x, uv_y, r49, 1.0
halt
tint:
mul r49, r48, 0.125
ret
`,
    tokens: ["switch (pc)", "call_stack", "textureStore"]
  },
  {
    name: "textures channel metadata and live inputs",
    main: `.include <std/screen.inc>
tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y
texel tex1_r, tex1_g, tex1_b, tex1_a, 1, px, py
chdim tmp0, tmp1, 0
chtime tmp2, 0
chsrate tmp3, 0
key tmp4, 4
mbtn tmp5, 0
mwheel tmp6, tmp7
gbtn tmp8, 0
gaxis tmp9, 0
out tex0_r, tmp4, tmp6, tex0_a
`,
    tokens: ["ast_channel_load", "ast_channel_meta", "ast_key_state", "ast_gamepad_axis_state"]
  },
  {
    name: "local include and const block",
    main: `.include "math.inc"
out shade, doubled, 0.0, 1.0
`,
    files: {
      "math.inc": `.consts
mov base, 0.25
add doubled, base, base
.end
.alias shade, r48
mov shade, doubled
`
    },
    tokens: ["ast_unorm(0.5)", "textureStore"]
  }
];

function compileNative(fixture: Fixture): string {
  const dir = mkdtempSync(join(tmpdir(), "asm-shader-toy-parity-"));
  try {
    const mainPath = join(dir, "main.asm");
    const outputPath = join(dir, "out.wgsl");
    writeFileSync(mainPath, fixture.main);
    for (const [path, content] of Object.entries(fixture.files ?? {})) {
      writeFileSync(join(dir, path), content);
    }
    const result = spawnSync(nativeCli, [mainPath, "--emit-wgsl", outputPath], {
      encoding: "utf8"
    });
    expect(result.status, result.stderr).toBe(0);
    return readFileSync(outputPath, "utf8");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

function compileBrowser(fixture: Fixture): CompileResult {
  return compileAsmToWgsl(
    [
      { path: "main.asm", content: fixture.main },
      ...Object.entries(fixture.files ?? {}).map(([path, content]) => ({ path, content }))
    ],
    "main.asm"
  );
}

parityDescribe("native and browser WGSL compiler parity", () => {
  for (const fixture of fixtures) {
    test(`compiles ${fixture.name}`, () => {
      const native = compileNative(fixture);
      const browser = compileBrowser(fixture);

      expect(browser.diagnostics).toEqual([]);
      for (const token of fixture.tokens) {
        expect(native).toContain(token);
        expect(browser.wgsl).toContain(token);
      }
    });
  }
});
