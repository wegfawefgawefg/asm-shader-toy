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

  test("reports unsupported texture ops for this prototype", () => {
    const result = compileAsmToWgsl(
      [
        {
          path: "main.asm",
          content: "tex r16, r17, r18, r19, 0, 0.5, 0.5\n"
        }
      ],
      "main.asm"
    );

    expect(result.diagnostics.map((diagnostic) => diagnostic.message)).toContain(
      "opcode 'tex' is not supported by the browser compiler yet"
    );
  });
});
