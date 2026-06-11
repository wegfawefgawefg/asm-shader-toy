import { StreamLanguage, syntaxHighlighting, HighlightStyle } from "@codemirror/language";
import { tags } from "@lezer/highlight";
import { basicSetup, EditorView } from "codemirror";

const opcodes = new Set([
  "mov",
  "add",
  "sub",
  "mul",
  "div",
  "sin",
  "cos",
  "sqrt",
  "abs",
  "floor",
  "fract",
  "min",
  "max",
  "mod",
  "norm",
  "lt",
  "gt",
  "eq",
  "jmp",
  "jnz",
  "jz",
  "jeq",
  "jne",
  "jlt",
  "jle",
  "jgt",
  "jge",
  "call",
  "ret",
  "tex",
  "texel",
  "chdim",
  "chtime",
  "chsrate",
  "key",
  "mbtn",
  "mwheel",
  "gbtn",
  "gaxis",
  "out",
  "out8",
  "halt"
]);

const inputs = new Set([
  "px",
  "py",
  "time",
  "width",
  "height",
  "mouse_x",
  "mouse_y",
  "mouse_down",
  "mouse_click_x",
  "mouse_click_y",
  "frame",
  "time_delta",
  "wall_clock_seconds",
  "year",
  "month",
  "day"
]);

const stdAliases = new Set([
  "uv_x",
  "uv_y",
  "pos_x",
  "pos_y",
  "color_r",
  "color_g",
  "color_b",
  "color_a",
  "tex0_r",
  "tex0_g",
  "tex0_b",
  "tex0_a",
  "tex1_r",
  "tex1_g",
  "tex1_b",
  "tex1_a",
  "tmp0",
  "tmp1",
  "tmp2",
  "tmp3",
  "tmp4",
  "tmp5",
  "tmp6",
  "tmp7",
  "tmp8",
  "tmp9",
  "tmp10",
  "tmp11",
  "tmp12",
  "tmp13",
  "tmp14",
  "tmp15"
]);

const asmLanguage = StreamLanguage.define({
  token(stream) {
    if (stream.eatSpace()) {
      return null;
    }
    if (stream.peek() === ";") {
      stream.skipToEnd();
      return "comment";
    }
    if (stream.match(/\.include|\.alias|\.consts|\.const|\.end/)) {
      return "keyword";
    }
    if (stream.match(/<[^>]+>|"[^"]+"/)) {
      return "string";
    }
    if (stream.match(/[A-Za-z_][A-Za-z0-9_]*:/)) {
      return "labelName";
    }
    if (stream.match(/-?(?:\d+\.\d*|\.\d+|\d+)(?:e[+-]?\d+)?/i)) {
      return "number";
    }
    if (stream.match(/r\d+/)) {
      return "variableName.special";
    }
    const word = stream.match(/[A-Za-z_][A-Za-z0-9_]*/, false);
    if (word && word !== true) {
      const token = word[0].toLowerCase();
      stream.match(/[A-Za-z_][A-Za-z0-9_]*/);
      if (opcodes.has(token)) {
        return "operatorKeyword";
      }
      if (inputs.has(token)) {
        return "atom";
      }
      if (stdAliases.has(token)) {
        return "variableName";
      }
      return "name";
    }
    stream.next();
    return null;
  }
});

const asmHighlight = HighlightStyle.define([
  { tag: tags.comment, color: "#6f7a85", fontStyle: "italic" },
  { tag: tags.keyword, color: "#ffcb6b" },
  { tag: tags.operatorKeyword, color: "#82aaff" },
  { tag: tags.labelName, color: "#c792ea" },
  { tag: tags.number, color: "#f78c6c" },
  { tag: tags.string, color: "#c3e88d" },
  { tag: tags.atom, color: "#89ddff" },
  { tag: tags.variableName, color: "#d7ff65" },
  { tag: tags.special(tags.variableName), color: "#ff5370" },
  { tag: tags.name, color: "#e6e8ea" }
]);

export function createAsmEditor(parent: HTMLElement, onChange: () => void): EditorView {
  return new EditorView({
    parent,
    doc: "",
    extensions: [
      basicSetup,
      asmLanguage,
      syntaxHighlighting(asmHighlight),
      EditorView.updateListener.of((update) => {
        if (update.docChanged) {
          onChange();
        }
      }),
      EditorView.theme({
        "&": {
          height: "100%",
          minHeight: "0",
          backgroundColor: "#101317",
          color: "#e6e8ea",
          fontSize: "13px"
        },
        ".cm-scroller": {
          fontFamily: '"SFMono-Regular", Consolas, "Liberation Mono", monospace',
          lineHeight: "1.45"
        },
        ".cm-content": {
          padding: "12px"
        },
        ".cm-gutters": {
          backgroundColor: "#101317",
          color: "#59626d",
          borderRight: "1px solid #2b3138"
        },
        ".cm-activeLine": {
          backgroundColor: "#171d23"
        },
        ".cm-activeLineGutter": {
          backgroundColor: "#171d23"
        },
        ".cm-selectionBackground": {
          backgroundColor: "#31475f !important"
        }
      })
    ]
  });
}

export function setEditorText(view: EditorView, text: string): void {
  view.dispatch({
    changes: { from: 0, to: view.state.doc.length, insert: text }
  });
}
