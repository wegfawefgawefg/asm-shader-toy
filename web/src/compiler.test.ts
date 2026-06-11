import { describe, expect, test } from "vitest";
import { compileAsmToWgsl } from "./compiler";

describe("browser asm compiler", () => {
  test("compiles std screen aliases and output", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: `.include <std/screen.inc>
out uv_x, uv_y, 0.5, 1.0
`
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("r[16]");
    expect(result.wgsl).toContain("textureStore");
  });

  test("compiles labels branches calls and returns", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: `mov r16, 0
loop:
add r16, r16, 1
jlt r16, 4, loop
call tint
out8 r16, 0, 0, 255
halt
tint:
add r16, r16, 10
ret
`
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("call_stack[call_depth]");
    expect(result.wgsl).toContain("pc = call_stack[call_depth]");
  });

  test("compiles texture and channel metadata ops", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: `tex r16, r17, r18, r19, 0, 0.5, 0.5
texel r20, r21, r22, r23, 1, 4, 5
chdim r24, r25, 0
chtime r26, 0
chsrate r27, 0
out r16, r21, r26, r19
`
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("ast_channel_load");
    expect(result.wgsl).toContain("textureLoad(channel0_texture");
    expect(result.wgsl).toContain("chdim_meta");
    expect(result.wgsl).toContain("let tex_channel_0 = 0;");
    expect(result.wgsl).toContain("ast_channel_meta(0);");
  });

  test("compiles live input query ops", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: `key r16, 4
mbtn r17, 1
mwheel r18, r19
gbtn r20, 0
gaxis r21, 2
out8 r16, r17, r18, r19
`
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("ast_key_state");
    expect(result.wgsl).toContain("ast_mouse_button_state");
    expect(result.wgsl).toContain("ast_gamepad_axis_state");
  });

  test("compiles .const values", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: `.const blue 0.25
out blue, 0.0, 0.0, 1.0
`
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("ast_unorm(0.25)");
  });

  test("compiles .consts math exports", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: `.consts
mov pi, 3.14159265
add tau, pi, pi
mul half_tau, tau, 0.5
.end
out half_tau, tau, 0.0, 1.0
`
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("ast_unorm(3.14159265)");
    expect(result.wgsl).toContain("ast_unorm(6.2831853)");
  });

  test("compiles .consts control flow and calls", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: `.consts
main:
mov i, 0
mov acc, 0
loop:
add acc, acc, 0.25
add i, i, 1
jlt i, 4, loop
call scale
halt
scale:
mul value, acc, 128
ret
.end
out8 value, 0, 0, 255
`
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("ast_byte(128.0)");
  });

  test("compiles comma aliases from local includes", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: `.include "aliases.inc"
out shade, 0.0, 0.0, 1.0
`
        },
        {
          path: "aliases.inc",
          content: `.alias shade, r48
mov shade, 0.5
`
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("r[48]");
    expect(result.wgsl).toContain("ast_unorm(r[48])");
  });

  test("normalizes relative include paths", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "examples/basics/main.asm",
          content: `.include "../common/math.inc"
out tau, 0.0, 0.0, 1.0
`
        },
        {
          path: "examples/common/math.inc",
          content: `.const tau 6.2831853
`
        }
      ],
      "examples/basics/main.asm"
    );

    expect(result.diagnostics).toEqual([]);
    expect(result.wgsl).toContain("ast_unorm(6.2831853)");
  });

  test("compiles a feedback buffer project pair", () => {
    const files = [
      {
        path: "image.asm",
        content: `.include <std/screen.inc>
tex tex0_r, tex0_g, tex0_b, tex0_a, 0, uv_x, uv_y
out tex0_r, tex0_r, tex0_r, 1.0
`
      },
      {
        path: "buffer.asm",
        content: `.include <std/aliases.inc>
texel tex0_r, tex0_g, tex0_b, tex0_a, 0, px, py
add tmp0, tex0_r, 0.01
fract tmp0, tmp0
out tmp0, tmp0, tmp0, 1.0
`
      }
    ];

    const image = compileAsmToWgsl(files, "image.asm");
    const buffer = compileAsmToWgsl(files, "buffer.asm");

    expect(image.diagnostics).toEqual([]);
    expect(buffer.diagnostics).toEqual([]);
    expect(buffer.wgsl).toContain("textureLoad(channel0_texture");
    expect(buffer.wgsl).toContain("textureStore");
  });
});
