import "./styles.css";
import { createAsmEditor, createWgslEditor, setEditorText } from "./asmEditor";
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
import { makeTemplateProject, templateProjects } from "./templates";
import {
  createProgram,
  destroyProgram,
  initWebGpu,
  renderFrame,
  type BrowserInputState,
  type ChannelRuntimeSource,
  type GpuContext,
  type ProgramState
} from "./webgpu";

type AppState = {
  project: ProjectBundle;
  selectedFile: string;
  running: boolean;
  showWgsl: boolean;
  frame: number;
  startSeconds: number;
  fps: number;
  fpsFrames: number;
  fpsStart: number;
};

type LiveChannelSource = ChannelRuntimeSource & {
  audioContext?: AudioContext;
  audioSource?: AudioBufferSourceNode;
  objectUrl?: string;
  stream?: MediaStream;
};

const app = document.querySelector<HTMLDivElement>("#app");
if (!app) {
  throw new Error("missing app root");
}
const appRoot: HTMLDivElement = app;

void main().catch((error) => {
  appRoot.textContent = error instanceof Error ? error.message : String(error);
});

async function main(): Promise<void> {
  const hashParams = new URLSearchParams(location.hash.startsWith("#") ? location.hash.slice(1) : location.hash);
  const defaultTemplate = templateProjects.find((candidate) => candidate.id === "examples/basics/plasma.asm");
  const defaultProject = defaultTemplate ? makeTemplateProject(defaultTemplate) : makeDefaultProject();
  const state: AppState = {
    project: defaultProject,
    selectedFile: defaultProject.settings.main,
    running: true,
    showWgsl: false,
    frame: 0,
    startSeconds: performance.now() / 1000,
    fps: 0,
    fpsFrames: 0,
    fpsStart: performance.now() / 1000
  };
  const channelSources = new Map<number, LiveChannelSource>();
  const inputState: BrowserInputState = {
    mouseX: 0,
    mouseY: 0,
    mouseDown: 0,
    mouseClickX: 0,
    mouseClickY: 0,
    mouseButtons: Array.from({ length: 8 }, () => 0),
    mouseWheelX: 0,
    mouseWheelY: 0
  };

  const loaded = await decodeProject(location.hash);
  if (loaded) {
    state.project = normalizeProject(loaded);
    state.selectedFile = loaded.settings.main;
  } else {
    const template = templateProjects.find((candidate) => candidate.id === hashParams.get("template"));
    if (template) {
      state.project = makeTemplateProject(template);
      state.selectedFile = template.main;
    }
  }

appRoot.innerHTML = `
  <main class="shell">
    <aside class="sidebar">
      <div class="brand">ASM Shader Toy</div>
      <section class="sidebar-section">
        <div class="section-title">Files</div>
        <div class="file-list"></div>
      </section>
      <section class="sidebar-section">
        <label class="template-picker">
          <span class="section-title">Templates</span>
          <select data-template>
            <option value="">Choose template...</option>
          </select>
        </label>
        <div class="action-grid">
          <button class="button" data-action="add-file">New</button>
          <button class="button" data-action="export">Export</button>
          <label class="button file-button">
            Import
            <input class="hidden-input" type="file" accept="application/json" data-import />
          </label>
          <button class="button" data-action="share">Share</button>
        </div>
      </section>
      <div class="sidebar-scroll">
        <section class="control-section" data-buffers></section>
        <section class="control-section" data-channels></section>
      </div>
    </aside>
    <section class="editor-pane">
      <div class="toolbar">
        <label>Main <select data-main></select></label>
        <label>Size <select data-size></select></label>
        <label>Scale <input data-scale type="number" min="1" max="8" /></label>
        <button class="button primary" data-action="compile-asm">Compile ASM</button>
        <button class="button" data-action="run">Run WGSL</button>
        <button class="button" data-action="pause">Pause</button>
        <button class="button" data-action="reset">Reset</button>
        <button class="button" data-action="save-frame">Save Frame</button>
        <button class="button" data-action="copy-frame">Copy PNG</button>
        <button class="button" data-action="record-video">Record</button>
        <button class="button" data-action="toggle-wgsl">WGSL</button>
        <output data-fps>0 fps</output>
      </div>
      <div class="split" data-split>
        <section class="source-panel">
          <div class="panel-title">ASM Project</div>
          <div class="code-editor" data-asm></div>
        </section>
        <section class="source-panel wgsl-panel" data-wgsl-panel>
          <div class="panel-title">WGSL</div>
          <div class="code-editor" data-wgsl></div>
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
const asmEditorHost = appRoot.querySelector<HTMLDivElement>("[data-asm]")!;
const splitPane = appRoot.querySelector<HTMLDivElement>("[data-split]")!;
const wgslPanel = appRoot.querySelector<HTMLElement>("[data-wgsl-panel]")!;
const wgslEditorHost = appRoot.querySelector<HTMLDivElement>("[data-wgsl]")!;
const diagnostics = appRoot.querySelector<HTMLPreElement>("[data-diagnostics]")!;
const statusText = appRoot.querySelector<HTMLDivElement>("[data-status]")!;
const fpsText = appRoot.querySelector<HTMLOutputElement>("[data-fps]")!;
const canvas = appRoot.querySelector<HTMLCanvasElement>("[data-canvas]")!;
const bufferList = appRoot.querySelector<HTMLDivElement>("[data-buffers]")!;
const channelList = appRoot.querySelector<HTMLDivElement>("[data-channels]")!;
const templateSelect = appRoot.querySelector<HTMLSelectElement>("[data-template]")!;

function setDiagnostics(message = "No errors.", kind: "ok" | "error" = "ok"): void {
  diagnostics.textContent = message;
  diagnostics.dataset.kind = kind;
}

function setError(message: string): void {
  setDiagnostics(message, "error");
}
let suppressAsmChange = false;
let suppressWgslChange = false;
const asmEditor = createAsmEditor(asmEditorHost, () => {
  if (!suppressAsmChange) {
    saveCurrentFile();
    scheduleCompile(compileAsm);
  }
});
const wgslEditor = createWgslEditor(wgslEditorHost, () => {
  if (!suppressWgslChange) {
    saveCurrentFile();
    scheduleCompile(compileWgsl);
  }
});

for (const name of Object.keys(sizePresets)) {
  const option = document.createElement("option");
  option.value = name;
  option.textContent = name;
  sizeSelect.append(option);
}

function syncSizeSelect(): void {
  const size = state.project.settings.size;
  if (!(size in sizePresets) && !Array.from(sizeSelect.options).some((option) => option.value === size)) {
    const option = document.createElement("option");
    option.value = size;
    option.textContent = size;
    sizeSelect.append(option);
  }
  sizeSelect.value = size;
}

for (const template of templateProjects) {
  const option = document.createElement("option");
  option.value = template.id;
  option.textContent = template.name;
  templateSelect.append(option);
}

function currentFile() {
  return state.project.files.find((file) => file.path === state.selectedFile) ?? state.project.files[0];
}

function fileName(path: string): string {
  return path.split("/").at(-1) ?? path;
}

function fileDirectory(path: string): string {
  const slash = path.lastIndexOf("/");
  return slash >= 0 ? path.slice(0, slash) : "";
}

function bufferSettings() {
  state.project.settings.buffers ??= [null, null, null, null];
  while (state.project.settings.buffers.length < 4) {
    state.project.settings.buffers.push(null);
  }
  return state.project.settings.buffers;
}

function syncCanvasSize(): void {
  const size = parseSize(state.project.settings.size);
  canvas.width = size.width * state.project.settings.scale;
  canvas.height = size.height * state.project.settings.scale;
}

function shaderMousePosition(event: PointerEvent | MouseEvent): { x: number; y: number } {
  const rect = canvas.getBoundingClientRect();
  const size = parseSize(state.project.settings.size);
  const x = Math.max(0, Math.min(size.width - 1, ((event.clientX - rect.left) / rect.width) * size.width));
  const y = Math.max(0, Math.min(size.height - 1, ((event.clientY - rect.top) / rect.height) * size.height));
  return { x, y };
}

function setMousePosition(event: PointerEvent | MouseEvent): void {
  const position = shaderMousePosition(event);
  inputState.mouseX = position.x;
  inputState.mouseY = position.y;
}

function renderProjectUi(): void {
  fileList.replaceChildren();
  for (const file of state.project.files) {
    const button = document.createElement("button");
    button.className = file.path === state.selectedFile ? "file selected" : "file";
    button.title = file.path;
    const icon = document.createElement("span");
    icon.className = "file-icon";
    icon.textContent = file.path.endsWith(".inc") ? "#" : "[]";
    const label = document.createElement("span");
    label.className = "file-name";
    label.textContent = fileName(file.path);
    const directory = document.createElement("span");
    directory.className = "file-path";
    directory.textContent = fileDirectory(file.path);
    button.append(icon, label, directory);
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
  syncSizeSelect();
  scaleInput.value = String(state.project.settings.scale);
  suppressAsmChange = true;
  setEditorText(asmEditor, currentFile().content);
  suppressAsmChange = false;
  suppressWgslChange = true;
  setEditorText(wgslEditor, state.project.settings.wgsl);
  suppressWgslChange = false;
  splitPane.classList.toggle("show-wgsl", state.showWgsl);
  wgslPanel.hidden = !state.showWgsl;
  renderBufferControls();
  renderChannelControls();
  syncCanvasSize();
}

function renderBufferControls(): void {
  bufferList.replaceChildren();
  const title = document.createElement("h2");
  title.className = "control-title";
  title.textContent = "Buffers";
  bufferList.append(title);
  bufferSettings().forEach((buffer, index) => {
    const label = document.createElement("label");
    label.className = "buffer";
    label.textContent = `buffer${index}`;
    const select = document.createElement("select");
    select.dataset.buffer = String(index);
    const none = document.createElement("option");
    none.value = "";
    none.textContent = "none";
    select.append(none);
    for (const file of state.project.files) {
      const option = document.createElement("option");
      option.value = file.path;
      option.textContent = file.path;
      select.append(option);
    }
    select.value = buffer?.file ?? "";
    label.append(select);
    bufferList.append(label);
  });
}

function renderChannelControls(): void {
  channelList.replaceChildren();
  const title = document.createElement("h2");
  title.className = "control-title";
  title.textContent = "Channels";
  channelList.append(title);
  state.project.settings.channels.forEach((channel, index) => {
    const wrapper = document.createElement("div");
    wrapper.className = "channel";
    const label = document.createElement("label");
    label.className = "channel-label";
    label.textContent = `channel${index}`;
    const controls = document.createElement("div");
    controls.className = "channel-controls";
    const imageLabel = document.createElement("label");
    imageLabel.className = "button channel-button channel-file-button";
    imageLabel.textContent = "Image";
    const input = document.createElement("input");
    input.type = "file";
    input.accept = "image/png,image/jpeg,image/webp,image/gif";
    input.dataset.channel = String(index);
    input.className = "hidden-input";
    imageLabel.append(input);
    const seedInput = document.createElement("input");
    seedInput.type = "text";
    seedInput.value = channel.seed ?? `seed${index}`;
    seedInput.dataset.noiseSeed = String(index);
    seedInput.className = "channel-seed";
    const noiseButton = document.createElement("button");
    noiseButton.className = "button channel-button";
    noiseButton.type = "button";
    noiseButton.dataset.action = "noise-channel";
    noiseButton.dataset.channel = String(index);
    noiseButton.textContent = "Noise";
    const webcamButton = document.createElement("button");
    webcamButton.className = "button channel-button";
    webcamButton.type = "button";
    webcamButton.dataset.action = "webcam-channel";
    webcamButton.dataset.channel = String(index);
    webcamButton.textContent = "Cam";
    const micButton = document.createElement("button");
    micButton.className = "button channel-button";
    micButton.type = "button";
    micButton.dataset.action = "mic-channel";
    micButton.dataset.channel = String(index);
    micButton.textContent = "Mic";
    const audioLabel = document.createElement("label");
    audioLabel.className = "button channel-button channel-file-button";
    audioLabel.textContent = "Aud";
    const audioInput = document.createElement("input");
    audioInput.className = "hidden-input";
    audioInput.type = "file";
    audioInput.accept = "audio/wav,audio/mpeg,audio/ogg,audio/flac,audio/mp4";
    audioInput.dataset.audioChannel = String(index);
    audioLabel.append(audioInput);
    const videoLabel = document.createElement("label");
    videoLabel.className = "button channel-button channel-file-button";
    videoLabel.textContent = "Vid";
    const videoInput = document.createElement("input");
    videoInput.className = "hidden-input";
    videoInput.type = "file";
    videoInput.accept = "video/mp4,video/webm,video/ogg";
    videoInput.dataset.videoChannel = String(index);
    videoLabel.append(videoInput);
    const videoUrlButton = document.createElement("button");
    videoUrlButton.className = "button channel-button";
    videoUrlButton.type = "button";
    videoUrlButton.dataset.action = "video-url-channel";
    videoUrlButton.dataset.channel = String(index);
    videoUrlButton.textContent = "URL";
    controls.append(
      imageLabel,
      seedInput,
      noiseButton,
      webcamButton,
      micButton,
      audioLabel,
      videoLabel,
      videoUrlButton
    );
    const meta = document.createElement("div");
    meta.className = "channel-meta";
    if (channel.kind === "noise" || channel.seed) {
      meta.textContent = `${channel.width}x${channel.height} noise:${channel.seed ?? channel.name}`;
    } else if (channel.kind === "webcam") {
      meta.textContent = channelSources.has(index)
        ? `${channel.width}x${channel.height} webcam mirrored`
        : "webcam disconnected";
    } else if (channel.kind === "microphone") {
      meta.textContent = channelSources.has(index)
        ? `${channel.width}x${channel.height} mic ${channel.sampleRate ?? 0}hz`
        : "microphone disconnected";
    } else if (channel.kind === "video") {
      meta.textContent =
        channelSources.has(index) || channel.sourceUrl ? `${channel.width}x${channel.height} ${channel.name}` : "video disconnected";
    } else if (channel.kind === "audio") {
      meta.textContent = channelSources.has(index)
        ? `${channel.width}x${channel.height} ${channel.name} ${channel.sampleRate ?? 0}hz`
        : "audio disconnected";
    } else if (channel.imageDataUrl || channel.sourceUrl) {
      meta.textContent = `${channel.width}x${channel.height} ${channel.name}`;
    } else {
      meta.textContent = "fallback 1x1";
    }
    label.append(input);
    wrapper.append(label, controls, meta);
    channelList.append(wrapper);
  });
}

function saveCurrentFile(): void {
  const file = currentFile();
  file.content = asmEditor.state.doc.toString();
  state.project.settings.wgsl = wgslEditor.state.doc.toString();
}

let gpuContext: GpuContext | null = null;
let program: ProgramState | null = null;
let rendererError = "";
let programBuildVersion = 0;
await restoreProjectRuntimeSources();
renderProjectUi();
try {
  gpuContext = await initWebGpu(canvas);
  program = await createProgram(gpuContext, state.project.settings.wgsl, state.project.settings, channelSources);
  setDiagnostics();
  statusText.textContent = `WebGPU ready (${gpuContext.adapterLabel})`;
} catch (error) {
  rendererError = error instanceof Error ? error.message : String(error);
  setError(`${rendererError}

The editor and ASM compiler are still usable, but this browser could not start the WebGPU renderer.`);
  statusText.textContent = "WebGPU unavailable";
}
let compileTimer: number | undefined;

function scheduleCompile(task: () => Promise<unknown>): void {
  if (compileTimer !== undefined) {
    window.clearTimeout(compileTimer);
  }
  compileTimer = window.setTimeout(() => {
    compileTimer = undefined;
    void task();
  }, 350);
}

async function waitForSubmittedWork(context: GpuContext): Promise<void> {
  try {
    await context.device.queue.onSubmittedWorkDone();
  } catch {
    // Device errors are reported through the uncaptured-error path below.
  }
}

async function replaceProgram(status: string): Promise<void> {
  if (!gpuContext) {
    setError(rendererError || "WebGPU is not available.");
    statusText.textContent = "WebGPU unavailable";
    return;
  }
  const context = gpuContext;
  const oldProgram = program;
  const version = ++programBuildVersion;
  statusText.textContent = "Building WGSL...";
  const nextProgram = await createProgram(context, state.project.settings.wgsl, state.project.settings, channelSources);
  if (version === programBuildVersion) {
    program = nextProgram;
    await waitForSubmittedWork(context);
    destroyProgram(oldProgram);
    if (version === programBuildVersion) {
      setDiagnostics();
      statusText.textContent = status;
    }
    return;
  }
  await waitForSubmittedWork(context);
  destroyProgram(nextProgram);
}

async function compileWgsl(): Promise<boolean> {
  saveCurrentFile();
  if (!gpuContext) {
    setError(rendererError || "WebGPU is not available.");
    statusText.textContent = "WebGPU unavailable";
    return false;
  }
  try {
    await replaceProgram("Compiled WGSL");
    return true;
  } catch (error) {
    setError(error instanceof Error ? error.message : String(error));
    statusText.textContent = "WGSL compile failed";
    return false;
  }
}

async function resetProgram(): Promise<void> {
  if (!gpuContext) {
    setError(rendererError || "WebGPU is not available.");
    statusText.textContent = "WebGPU unavailable";
    return;
  }
  state.frame = 0;
  state.startSeconds = performance.now() / 1000;
  state.fps = 0;
  state.fpsFrames = 0;
  state.fpsStart = state.startSeconds;
  await replaceProgram("Reset");
  fpsText.textContent = "0 fps";
}

function saveFrame(): void {
  canvas.toBlob((blob) => {
    if (!blob) {
      statusText.textContent = "Save frame failed";
      return;
    }
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = `asm-shader-toy-frame-${state.frame}.png`;
    link.click();
    URL.revokeObjectURL(link.href);
    statusText.textContent = "Saved frame";
  }, "image/png");
}

function canvasBlob(type: string): Promise<Blob> {
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => {
      if (blob) {
        resolve(blob);
      } else {
        reject(new Error(`could not encode ${type}`));
      }
    }, type);
  });
}

async function copyFramePng(): Promise<void> {
  if (!navigator.clipboard || !("ClipboardItem" in window)) {
    throw new Error("PNG clipboard writes are not available in this browser.");
  }
  const blob = await canvasBlob("image/png");
  await navigator.clipboard.write([new ClipboardItem({ "image/png": blob })]);
  statusText.textContent = "Copied PNG";
}

function downloadBlob(blob: Blob, filename: string): void {
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = filename;
  link.click();
  URL.revokeObjectURL(link.href);
}

async function recordVideo(seconds: number): Promise<void> {
  if (!("MediaRecorder" in window) || !canvas.captureStream) {
    throw new Error("Canvas video recording is not available in this browser.");
  }
  const duration = Math.max(0.1, Math.min(300, seconds));
  const stream = canvas.captureStream(60);
  const mimeType = MediaRecorder.isTypeSupported("video/webm;codecs=vp9")
    ? "video/webm;codecs=vp9"
    : MediaRecorder.isTypeSupported("video/webm;codecs=vp8")
      ? "video/webm;codecs=vp8"
      : "video/webm";
  const recorder = new MediaRecorder(stream, { mimeType });
  const chunks: Blob[] = [];
  recorder.addEventListener("dataavailable", (event) => {
    if (event.data.size > 0) {
      chunks.push(event.data);
    }
  });
  try {
    await new Promise<void>((resolve, reject) => {
      recorder.addEventListener("stop", () => resolve(), { once: true });
      recorder.addEventListener("error", () => reject(new Error("video recording failed")), { once: true });
      recorder.start();
      window.setTimeout(() => recorder.stop(), Math.round(duration * 1000));
    });
  } finally {
    stream.getTracks().forEach((track) => track.stop());
  }
  downloadBlob(new Blob(chunks, { type: mimeType }), `asm-shader-toy-${duration.toFixed(1)}s.webm`);
  statusText.textContent = `Saved ${duration.toFixed(1)}s video`;
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

function stopChannelSource(index: number): void {
  const source = channelSources.get(index);
  if (!source) {
    return;
  }
  source.video?.pause();
  source.audioSource?.stop();
  if (source.objectUrl) {
    URL.revokeObjectURL(source.objectUrl);
  }
  void source.audioContext?.close();
  source.stream?.getTracks().forEach((track) => track.stop());
  channelSources.delete(index);
}

function resumeAudioContexts(): void {
  for (const source of channelSources.values()) {
    if (source.audioContext?.state === "suspended") {
      void source.audioContext.resume();
    }
  }
}

async function setChannelImage(index: number, file: File): Promise<void> {
  stopChannelSource(index);
  const imageDataUrl = await dataUrlForFile(file);
  const dimensions = await imageDimensions(imageDataUrl);
  const channel: ChannelSetting = {
    kind: "image",
    name: file.name,
    width: dimensions.width,
    height: dimensions.height,
    imageDataUrl
  };
  state.project.settings.channels[index] = channel;
  renderChannelControls();
  await compileWgsl();
}

async function setChannelNoise(index: number, seed: string): Promise<void> {
  stopChannelSource(index);
  const normalizedSeed = seed.trim() || `seed${index}`;
  state.project.settings.channels[index] = {
    kind: "noise",
    name: `noise:${normalizedSeed}`,
    width: 256,
    height: 256,
    seed: normalizedSeed
  };
  renderChannelControls();
  await compileWgsl();
}

async function setChannelWebcam(index: number): Promise<void> {
  stopChannelSource(index);
  if (!navigator.mediaDevices?.getUserMedia) {
    throw new Error("getUserMedia is not available in this browser.");
  }
  const stream = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
  const video = document.createElement("video");
  video.muted = true;
  video.playsInline = true;
  video.srcObject = stream;
  await new Promise<void>((resolve, reject) => {
    video.addEventListener("loadedmetadata", () => resolve(), { once: true });
    video.addEventListener("error", () => reject(video.error ?? new Error("webcam video failed")), { once: true });
  });
  await video.play();
  channelSources.set(index, { video, stream, mirrored: true });
  state.project.settings.channels[index] = {
    kind: "webcam",
    name: "webcam",
    width: Math.max(1, video.videoWidth),
    height: Math.max(1, video.videoHeight)
  };
  renderChannelControls();
  await compileWgsl();
}

async function setChannelMicrophone(index: number): Promise<void> {
  stopChannelSource(index);
  if (!navigator.mediaDevices?.getUserMedia) {
    throw new Error("getUserMedia is not available in this browser.");
  }
  const stream = await navigator.mediaDevices.getUserMedia({ video: false, audio: true });
  const audioContext = new AudioContext();
  const sourceNode = audioContext.createMediaStreamSource(stream);
  const analyser = audioContext.createAnalyser();
  analyser.fftSize = 1024;
  analyser.smoothingTimeConstant = 0.35;
  sourceNode.connect(analyser);
  const width = 512;
  const height = 2;
  channelSources.set(index, {
    audioContext,
    stream,
    audio: {
      analyser,
      timeData: new Uint8Array(new ArrayBuffer(width)),
      frequencyData: new Uint8Array(new ArrayBuffer(width)),
      pixels: new Uint8Array(new ArrayBuffer(width * height * 4)),
      width,
      height
    }
  });
  state.project.settings.channels[index] = {
    kind: "microphone",
    name: "microphone",
    width,
    height,
    sampleRate: audioContext.sampleRate
  };
  renderChannelControls();
  await compileWgsl();
}

async function setChannelAudio(index: number, file: File): Promise<void> {
  stopChannelSource(index);
  const audioContext = new AudioContext();
  const audioBuffer = await audioContext.decodeAudioData(await file.arrayBuffer());
  await setChannelAudioBuffer(index, file.name, audioContext, audioBuffer);
}

async function setChannelAudioUrl(index: number, url: string, name: string, compile = true): Promise<void> {
  stopChannelSource(index);
  const audioContext = new AudioContext();
  const response = await fetch(url);
  const audioBuffer = await audioContext.decodeAudioData(await response.arrayBuffer());
  await setChannelAudioBuffer(index, name, audioContext, audioBuffer, url, compile);
}

async function setChannelAudioBuffer(
  index: number,
  name: string,
  audioContext: AudioContext,
  audioBuffer: AudioBuffer,
  sourceUrl?: string,
  compile = true
): Promise<void> {
  const sourceNode = audioContext.createBufferSource();
  const analyser = audioContext.createAnalyser();
  const silentGain = audioContext.createGain();
  silentGain.gain.value = 0;
  analyser.fftSize = 1024;
  analyser.smoothingTimeConstant = 0.35;
  sourceNode.buffer = audioBuffer;
  sourceNode.loop = true;
  sourceNode.connect(analyser);
  analyser.connect(silentGain);
  silentGain.connect(audioContext.destination);
  sourceNode.start();
  const width = 512;
  const height = 2;
  channelSources.set(index, {
    audioContext,
    audioSource: sourceNode,
    audio: {
      analyser,
      duration: audioBuffer.duration,
      timeData: new Uint8Array(new ArrayBuffer(width)),
      frequencyData: new Uint8Array(new ArrayBuffer(width)),
      pixels: new Uint8Array(new ArrayBuffer(width * height * 4)),
      startedAt: performance.now() / 1000,
      width,
      height
    }
  });
  state.project.settings.channels[index] = {
    kind: "audio",
    name,
    width,
    height,
    sampleRate: audioBuffer.sampleRate,
    sourceUrl
  };
  renderChannelControls();
  if (compile) {
    await compileWgsl();
  }
}

async function setChannelVideo(index: number, file: File): Promise<void> {
  stopChannelSource(index);
  const objectUrl = URL.createObjectURL(file);
  await setChannelVideoSource(index, objectUrl, file.name, objectUrl);
}

async function setChannelVideoUrl(index: number, url: string): Promise<void> {
  stopChannelSource(index);
  await setChannelVideoSource(index, url, url, undefined, url);
}

async function setChannelVideoSource(
  index: number,
  src: string,
  name: string,
  objectUrl?: string,
  sourceUrl?: string,
  compile = true
): Promise<void> {
  const video = document.createElement("video");
  video.muted = true;
  video.loop = true;
  video.playsInline = true;
  video.crossOrigin = "anonymous";
  video.src = src;
  await new Promise<void>((resolve, reject) => {
    video.addEventListener("loadedmetadata", () => resolve(), { once: true });
    video.addEventListener("error", () => reject(video.error ?? new Error("video failed to load")), { once: true });
  });
  await video.play();
  channelSources.set(index, { video, objectUrl });
  state.project.settings.channels[index] = {
    kind: "video",
    name,
    width: Math.max(1, video.videoWidth),
    height: Math.max(1, video.videoHeight),
    sourceUrl
  };
  renderChannelControls();
  if (compile) {
    await compileWgsl();
  }
}

async function restoreProjectRuntimeSources(): Promise<void> {
  const restores = state.project.settings.channels.map(async (channel, index) => {
    if (channel.kind === "video" && channel.sourceUrl) {
      stopChannelSource(index);
      await setChannelVideoSource(index, channel.sourceUrl, channel.name, undefined, channel.sourceUrl, false);
    }
    if (channel.kind === "audio" && channel.sourceUrl) {
      stopChannelSource(index);
      await setChannelAudioUrl(index, channel.sourceUrl, channel.name, false);
    }
  });
  await Promise.all(restores);
}


async function compileAsm(): Promise<boolean> {
  saveCurrentFile();
  setDiagnostics();
  statusText.textContent = "Compiling ASM...";
  const diagnosticsList: string[] = [];
  const maxSteps = state.project.settings.maxSteps ?? 4096;
  const imageResult = compileAsmToWgsl(state.project.files, state.project.settings.main, maxSteps);
  diagnosticsList.push(
    ...imageResult.diagnostics.map((diagnostic) => `${diagnostic.file}:${diagnostic.line}: ${diagnostic.message}`)
  );
  for (const buffer of bufferSettings()) {
    if (!buffer) {
      continue;
    }
    const result = compileAsmToWgsl(state.project.files, buffer.file, maxSteps);
    buffer.wgsl = result.wgsl;
    diagnosticsList.push(
      ...result.diagnostics.map((diagnostic) => `${diagnostic.file}:${diagnostic.line}: ${diagnostic.message}`)
    );
  }
  if (diagnosticsList.length > 0) {
    setError(diagnosticsList.join("\n"));
    statusText.textContent = "ASM compile failed";
    return false;
  }
  state.project.settings.wgsl = imageResult.wgsl;
  suppressWgslChange = true;
  setEditorText(wgslEditor, imageResult.wgsl);
  suppressWgslChange = false;
  return compileWgsl();
}

function currentSizeKey(): string {
  const size = parseSize(state.project.settings.size);
  return `${size.width}x${size.height}`;
}

function tick(): void {
  if (state.running && gpuContext && program && program.sizeKey === currentSizeKey()) {
    renderFrame(gpuContext, program, state.project.settings, state.frame, state.startSeconds, channelSources, inputState);
    inputState.mouseWheelX = 0;
    inputState.mouseWheelY = 0;
    if (gpuContext.errors.length > 0) {
      setError(gpuContext.errors.join("\n"));
      statusText.textContent = "WebGPU error";
      gpuContext.errors.length = 0;
    }
    state.frame += 1;
    state.fpsFrames += 1;
    const now = performance.now() / 1000;
    const elapsed = now - state.fpsStart;
    if (elapsed >= 0.5) {
      state.fps = state.fpsFrames / elapsed;
      state.fpsFrames = 0;
      state.fpsStart = now;
      fpsText.textContent = `${state.fps.toFixed(1)} fps`;
    }
  }
  requestAnimationFrame(tick);
}

async function loadTemplate(templateId: string): Promise<void> {
  const template = templateProjects.find((candidate) => candidate.id === templateId);
  if (!template) {
    return;
  }
  saveCurrentFile();
  for (const index of Array.from(channelSources.keys())) {
    stopChannelSource(index);
  }
  state.project = makeTemplateProject(template);
  state.selectedFile = state.project.settings.main;
  state.frame = 0;
  state.startSeconds = performance.now() / 1000;
  await restoreProjectRuntimeSources();
  renderProjectUi();
  statusText.textContent = `Loading ${template.name}...`;
  history.replaceState(null, "", `#template=${encodeURIComponent(template.id)}`);
  const loaded = await compileAsm();
  templateSelect.value = "";
  if (loaded) {
    statusText.textContent = `Loaded ${template.name}`;
  }
}

appRoot.addEventListener("input", (event) => {
  const target = event.target;
  if (target instanceof HTMLInputElement && target.dataset.channel !== undefined && target.files?.[0]) {
    void setChannelImage(Number(target.dataset.channel), target.files[0]);
    return;
  }
  if (target instanceof HTMLInputElement && target.dataset.videoChannel !== undefined && target.files?.[0]) {
    void setChannelVideo(Number(target.dataset.videoChannel), target.files[0]);
    return;
  }
  if (target instanceof HTMLInputElement && target.dataset.audioChannel !== undefined && target.files?.[0]) {
    void setChannelAudio(Number(target.dataset.audioChannel), target.files[0]);
    return;
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
    scheduleCompile(compileAsm);
  }
  if (target === templateSelect) {
    void loadTemplate(templateSelect.value).catch((error) => {
      setError(error instanceof Error ? error.message : String(error));
      statusText.textContent = "Template failed";
    });
  }
  if (target instanceof HTMLSelectElement && target.dataset.buffer !== undefined) {
    const index = Number(target.dataset.buffer);
    bufferSettings()[index] = target.value ? { file: target.value, wgsl: "" } : null;
    scheduleCompile(compileAsm);
  }
});

canvas.addEventListener("pointermove", (event) => {
  setMousePosition(event);
});

canvas.addEventListener("pointerdown", (event) => {
  canvas.setPointerCapture(event.pointerId);
  setMousePosition(event);
  inputState.mouseDown = 1;
  inputState.mouseClickX = inputState.mouseX;
  inputState.mouseClickY = inputState.mouseY;
  inputState.mouseButtons[Math.max(0, Math.min(7, event.button))] = 1;
});

canvas.addEventListener("pointerup", (event) => {
  setMousePosition(event);
  inputState.mouseButtons[Math.max(0, Math.min(7, event.button))] = 0;
  inputState.mouseDown = inputState.mouseButtons.some((button) => button !== 0) ? 1 : 0;
});

canvas.addEventListener("pointercancel", () => {
  inputState.mouseButtons.fill(0);
  inputState.mouseDown = 0;
});

canvas.addEventListener("pointerleave", () => {
  inputState.mouseDown = inputState.mouseButtons.some((button) => button !== 0) ? 1 : 0;
});

canvas.addEventListener("wheel", (event) => {
  inputState.mouseWheelX += event.deltaX;
  inputState.mouseWheelY += event.deltaY;
  event.preventDefault();
});

window.addEventListener("pointerdown", resumeAudioContexts);
window.addEventListener("keydown", resumeAudioContexts);

canvas.addEventListener("contextmenu", (event) => {
  event.preventDefault();
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
  if (action === "reset") {
    void resetProgram().catch((error) => {
      setError(error instanceof Error ? error.message : String(error));
      statusText.textContent = "Reset failed";
    });
  }
  if (action === "save-frame") {
    saveFrame();
  }
  if (action === "copy-frame") {
    void copyFramePng().catch((error) => {
      setError(error instanceof Error ? error.message : String(error));
      statusText.textContent = "Copy PNG failed";
    });
  }
  if (action === "record-video") {
    const duration = Number(window.prompt("Record seconds", "5") ?? "");
    if (Number.isFinite(duration) && duration > 0) {
      statusText.textContent = "Recording video...";
      void recordVideo(duration).catch((error) => {
        setError(error instanceof Error ? error.message : String(error));
        statusText.textContent = "Record failed";
      });
    }
  }
  if (action === "toggle-wgsl") {
    state.showWgsl = !state.showWgsl;
    renderProjectUi();
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
  if (action === "noise-channel") {
    const index = Number((event.target as HTMLElement).dataset.channel);
    const seed = appRoot.querySelector<HTMLInputElement>(`[data-noise-seed="${index}"]`)?.value ?? `seed${index}`;
    void setChannelNoise(index, seed);
  }
  if (action === "webcam-channel") {
    const index = Number((event.target as HTMLElement).dataset.channel);
    void setChannelWebcam(index).catch((error) => {
      setError(error instanceof Error ? error.message : String(error));
      statusText.textContent = "Webcam failed";
    });
  }
  if (action === "mic-channel") {
    const index = Number((event.target as HTMLElement).dataset.channel);
    void setChannelMicrophone(index).catch((error) => {
      setError(error instanceof Error ? error.message : String(error));
      statusText.textContent = "Microphone failed";
    });
  }
  if (action === "video-url-channel") {
    const index = Number((event.target as HTMLElement).dataset.channel);
    const current = state.project.settings.channels[index]?.sourceUrl ?? "";
    const url = window.prompt("Video URL", current)?.trim();
    if (url) {
      void setChannelVideoUrl(index, url).catch((error) => {
        setError(error instanceof Error ? error.message : String(error));
        statusText.textContent = "Video URL failed";
      });
    }
  }
});

appRoot.querySelector<HTMLInputElement>("[data-import]")!.addEventListener("change", async (event) => {
  const file = (event.target as HTMLInputElement).files?.[0];
  if (!file) {
    return;
  }
  state.project = normalizeProject(JSON.parse(await file.text()) as ProjectBundle);
  for (const index of Array.from(channelSources.keys())) {
    stopChannelSource(index);
  }
  state.selectedFile = state.project.settings.main;
  await restoreProjectRuntimeSources();
  renderProjectUi();
  await compileWgsl();
});

window.addEventListener("hashchange", async () => {
  const decoded = await decodeProject(location.hash);
  if (decoded) {
    state.project = normalizeProject(decoded);
    for (const index of Array.from(channelSources.keys())) {
      stopChannelSource(index);
    }
    state.selectedFile = decoded.settings.main;
    await restoreProjectRuntimeSources();
    renderProjectUi();
    await compileWgsl();
    return;
  }

  const params = new URLSearchParams(location.hash.startsWith("#") ? location.hash.slice(1) : location.hash);
  const templateId = params.get("template");
  if (templateId) {
    await loadTemplate(templateId);
  }
});
requestAnimationFrame(tick);
}
