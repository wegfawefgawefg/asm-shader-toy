import "./styles.css";
import { compileAsmToWgsl } from "./compiler";
import {
  decodeProject,
  encodeProject,
  makeDefaultProject,
  normalizeProject,
  parseSize,
  sizePresets,
  type ChannelSetting,
  type ProjectBundle
} from "./project";
import { createProgram, destroyProgram, initWebGpu, renderFrame } from "./webgpu";

type AppState = {
  project: ProjectBundle;
  selectedFile: string;
  running: boolean;
  frame: number;
  startSeconds: number;
};

const app = document.querySelector<HTMLDivElement>("#app");
if (!app) {
  throw new Error("missing app root");
}
const appRoot: HTMLDivElement = app;

void main();

async function main(): Promise<void> {
  const state: AppState = {
    project: makeDefaultProject(),
    selectedFile: "main.asm",
    running: true,
    frame: 0,
    startSeconds: performance.now() / 1000
  };

  const loaded = await decodeProject(location.hash);
  if (loaded) {
    state.project = normalizeProject(loaded);
    state.selectedFile = loaded.settings.main;
  }

appRoot.innerHTML = `
  <main class="shell">
    <aside class="sidebar">
      <div class="brand">ASM Shader Toy</div>
      <div class="file-list"></div>
      <button class="button" data-action="add-file">New File</button>
      <button class="button" data-action="export">Export JSON</button>
      <label class="button file-button">
        Import JSON
        <input class="hidden-input" type="file" accept="application/json" data-import />
      </label>
      <button class="button" data-action="share">Copy Share URL</button>
      <div class="channel-list" data-channels></div>
    </aside>
    <section class="editor-pane">
      <div class="toolbar">
        <label>Main <select data-main></select></label>
        <label>Size <select data-size></select></label>
        <label>Scale <input data-scale type="number" min="1" max="8" /></label>
        <button class="button primary" data-action="compile-asm">Compile ASM</button>
        <button class="button" data-action="run">Run WGSL</button>
        <button class="button" data-action="pause">Pause</button>
      </div>
      <div class="split">
        <section class="source-panel">
          <div class="panel-title">ASM Project</div>
          <textarea data-asm spellcheck="false"></textarea>
        </section>
        <section class="source-panel">
          <div class="panel-title">WGSL</div>
          <textarea data-wgsl spellcheck="false"></textarea>
        </section>
      </div>
      <pre class="diagnostics" data-diagnostics></pre>
    </section>
    <section class="preview-pane">
      <canvas data-canvas></canvas>
      <div class="status" data-status>Initializing WebGPU...</div>
    </section>
  </main>
`;

const fileList = appRoot.querySelector<HTMLDivElement>(".file-list")!;
const mainSelect = appRoot.querySelector<HTMLSelectElement>("[data-main]")!;
const sizeSelect = appRoot.querySelector<HTMLSelectElement>("[data-size]")!;
const scaleInput = appRoot.querySelector<HTMLInputElement>("[data-scale]")!;
const asmEditor = appRoot.querySelector<HTMLTextAreaElement>("[data-asm]")!;
const wgslEditor = appRoot.querySelector<HTMLTextAreaElement>("[data-wgsl]")!;
const diagnostics = appRoot.querySelector<HTMLPreElement>("[data-diagnostics]")!;
const statusText = appRoot.querySelector<HTMLDivElement>("[data-status]")!;
const canvas = appRoot.querySelector<HTMLCanvasElement>("[data-canvas]")!;
const channelList = appRoot.querySelector<HTMLDivElement>("[data-channels]")!;

for (const name of Object.keys(sizePresets)) {
  const option = document.createElement("option");
  option.value = name;
  option.textContent = name;
  sizeSelect.append(option);
}

function currentFile() {
  return state.project.files.find((file) => file.path === state.selectedFile) ?? state.project.files[0];
}

function syncCanvasSize(): void {
  const size = parseSize(state.project.settings.size);
  canvas.width = size.width * state.project.settings.scale;
  canvas.height = size.height * state.project.settings.scale;
}

function renderProjectUi(): void {
  fileList.replaceChildren();
  for (const file of state.project.files) {
    const button = document.createElement("button");
    button.className = file.path === state.selectedFile ? "file selected" : "file";
    button.textContent = file.path;
    button.addEventListener("click", () => {
      state.selectedFile = file.path;
      renderProjectUi();
    });
    fileList.append(button);
  }

  mainSelect.replaceChildren();
  for (const file of state.project.files) {
    const option = document.createElement("option");
    option.value = file.path;
    option.textContent = file.path;
    mainSelect.append(option);
  }
  mainSelect.value = state.project.settings.main;
  sizeSelect.value = state.project.settings.size;
  scaleInput.value = String(state.project.settings.scale);
  asmEditor.value = currentFile().content;
  wgslEditor.value = state.project.settings.wgsl;
  renderChannelControls();
  syncCanvasSize();
}

function renderChannelControls(): void {
  channelList.replaceChildren();
  state.project.settings.channels.forEach((channel, index) => {
    const wrapper = document.createElement("div");
    wrapper.className = "channel";
    const label = document.createElement("label");
    label.className = "channel-label";
    label.textContent = `channel${index}`;
    const input = document.createElement("input");
    input.type = "file";
    input.accept = "image/png,image/jpeg,image/webp,image/gif";
    input.dataset.channel = String(index);
    input.className = "channel-input";
    const meta = document.createElement("div");
    meta.className = "channel-meta";
    meta.textContent = channel.imageDataUrl ? `${channel.width}x${channel.height} ${channel.name}` : "fallback 1x1";
    label.append(input);
    wrapper.append(label, meta);
    channelList.append(wrapper);
  });
}

function saveCurrentFile(): void {
  const file = currentFile();
  file.content = asmEditor.value;
  state.project.settings.wgsl = wgslEditor.value;
}

let gpuContext = await initWebGpu(canvas);
let program = await createProgram(gpuContext, state.project.settings.wgsl, state.project.settings);
statusText.textContent = "WebGPU ready";

async function compileWgsl(): Promise<void> {
  saveCurrentFile();
  try {
    destroyProgram(program);
    program = await createProgram(gpuContext, state.project.settings.wgsl, state.project.settings);
    diagnostics.textContent = "";
    statusText.textContent = "Compiled WGSL";
  } catch (error) {
    diagnostics.textContent = error instanceof Error ? error.message : String(error);
    statusText.textContent = "WGSL compile failed";
  }
}

async function dataUrlForFile(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.addEventListener("load", () => resolve(String(reader.result)));
    reader.addEventListener("error", () => reject(reader.error ?? new Error("could not read image")));
    reader.readAsDataURL(file);
  });
}

async function imageDimensions(dataUrl: string): Promise<{ width: number; height: number }> {
  const response = await fetch(dataUrl);
  const blob = await response.blob();
  const image = await createImageBitmap(blob);
  const dimensions = { width: image.width, height: image.height };
  image.close();
  return dimensions;
}

async function setChannelImage(index: number, file: File): Promise<void> {
  const imageDataUrl = await dataUrlForFile(file);
  const dimensions = await imageDimensions(imageDataUrl);
  const channel: ChannelSetting = {
    name: file.name,
    width: dimensions.width,
    height: dimensions.height,
    imageDataUrl
  };
  state.project.settings.channels[index] = channel;
  renderChannelControls();
  await compileWgsl();
}

async function compileAsm(): Promise<void> {
  saveCurrentFile();
  const result = compileAsmToWgsl(state.project.files, state.project.settings.main);
  if (result.diagnostics.length > 0) {
    diagnostics.textContent = result.diagnostics
      .map((diagnostic) => `${diagnostic.file}:${diagnostic.line}: ${diagnostic.message}`)
      .join("\n");
    statusText.textContent = "ASM compile failed";
    return;
  }
  state.project.settings.wgsl = result.wgsl;
  wgslEditor.value = result.wgsl;
  await compileWgsl();
}

function tick(): void {
  if (state.running && program) {
    renderFrame(gpuContext, program, state.project.settings, state.frame, state.startSeconds);
    state.frame += 1;
  }
  requestAnimationFrame(tick);
}

appRoot.addEventListener("input", (event) => {
  const target = event.target;
  if (target instanceof HTMLInputElement && target.dataset.channel !== undefined && target.files?.[0]) {
    void setChannelImage(Number(target.dataset.channel), target.files[0]);
    return;
  }
  if (target === asmEditor || target === wgslEditor) {
    saveCurrentFile();
  }
  if (target === sizeSelect) {
    state.project.settings.size = sizeSelect.value as ProjectBundle["settings"]["size"];
    syncCanvasSize();
    void compileWgsl();
  }
  if (target === scaleInput) {
    state.project.settings.scale = Math.max(1, Number(scaleInput.value) || 1);
    syncCanvasSize();
  }
  if (target === mainSelect) {
    state.project.settings.main = mainSelect.value;
  }
});

appRoot.addEventListener("click", (event) => {
  const action = (event.target as HTMLElement).dataset.action;
  if (!action) {
    return;
  }
  if (action === "run") {
    void compileWgsl();
  }
  if (action === "compile-asm") {
    void compileAsm();
  }
  if (action === "pause") {
    state.running = !state.running;
    (event.target as HTMLButtonElement).textContent = state.running ? "Pause" : "Play";
  }
  if (action === "add-file") {
    saveCurrentFile();
    const path = `file${state.project.files.length}.asm`;
    state.project.files.push({ path, content: "" });
    state.selectedFile = path;
    renderProjectUi();
  }
  if (action === "export") {
    saveCurrentFile();
    const blob = new Blob([JSON.stringify(state.project, null, 2)], { type: "application/json" });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = "asm-shader-toy-project.json";
    link.click();
    URL.revokeObjectURL(link.href);
  }
  if (action === "share") {
    saveCurrentFile();
    void encodeProject(state.project).then((hash) => {
      const url = `${location.origin}${location.pathname}${hash}`;
      return navigator.clipboard.writeText(url).then(() => {
        statusText.textContent = "Share URL copied";
      });
    });
  }
});

appRoot.querySelector<HTMLInputElement>("[data-import]")!.addEventListener("change", async (event) => {
  const file = (event.target as HTMLInputElement).files?.[0];
  if (!file) {
    return;
  }
  state.project = normalizeProject(JSON.parse(await file.text()) as ProjectBundle);
  state.selectedFile = state.project.settings.main;
  renderProjectUi();
  await compileWgsl();
});

window.addEventListener("hashchange", async () => {
  const decoded = await decodeProject(location.hash);
  if (!decoded) {
    return;
  }
  state.project = normalizeProject(decoded);
  state.selectedFile = decoded.settings.main;
  renderProjectUi();
  await compileWgsl();
});

renderProjectUi();
requestAnimationFrame(tick);
}
