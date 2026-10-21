import type { MovieOptions } from "./movie-timeline";

export interface MovieResult { folder: string; movie: string | null; frames: number }
export interface MovieBridge {
  capabilities(): Promise<{ mp4: boolean; reason?: string }>;
  start(options: MovieOptions, metadata: unknown): Promise<{ folder: string; frameCount: number } | null>;
  append(index: number, png: Uint8Array): Promise<{ completedFrames: number }>;
  finish(): Promise<MovieResult>;
  cancel(): Promise<{ folder: string | null }>;
  reveal(): Promise<string | undefined>;
}
declare global { interface Window { kerrMovie?: MovieBridge } }

export interface MovieRenderer {
  events: readonly string[];
  currentEvent(): string | null;
  begin(options: MovieOptions): unknown;
  render(index: number): Promise<Uint8Array>;
  end(): void;
}

export function openMovieDialog(renderer: MovieRenderer): void {
  const existing = document.getElementById("movieExport") as HTMLDialogElement | null;
  if (existing) { existing.focus(); return; }
  const dialog = document.createElement("dialog");
  dialog.id = "movieExport";
  dialog.setAttribute("aria-labelledby", "movieTitle");
  dialog.innerHTML = `<h2 id="movieTitle">Render merger film</h2>
    <p>Full inspiral and collision, followed by the remnant. Clean image, no interface or audio.
    The merger lands around 80% of the clip. Your current camera is used when the selected event matches the preview.</p>
    <fieldset id="movieSettings">
      <label>Merger event <select id="movieEvent"></select></label>
      <label>Resolution <select id="movieResolution"><option value="1280x720">1280 × 720</option>
        <option value="1920x1080" selected>1920 × 1080 · Full HD</option>
        <option value="2560x1440">2560 × 1440 · QHD</option><option value="3840x2160">3840 × 2160 · 4K</option></select></label>
      <label>Frame rate <select id="movieFps"><option value="24">24 FPS</option><option value="30">30 FPS</option>
        <option value="60" selected>60 FPS · normal video</option><option value="144">144 FPS · 144 Hz display</option></select></label>
      <label>Duration in seconds <input id="movieDuration" type="number" min="1" max="120" step="1" value="12" required></label>
      <label>Save as <select id="movieFormat"><option value="mp4">MP4 + lossless PNG frames</option>
        <option value="png">Lossless PNG frames only</option></select></label>
    </fieldset>
    <p id="movieEstimate"></p>
    <p>Every frame renders at the selected resolution with fixed detail, sky and bloom.
    High resolution and 144 FPS can take substantial time. Playback at 144 FPS needs a compatible player and display setting.</p>
    <progress id="movieProgress" value="0" max="1"></progress>
    <p id="movieStatus" role="status" aria-live="polite">Checking video encoder…</p>
    <div class="movie-actions"><button id="movieReveal" hidden>Open output folder</button>
    <button id="movieStart" disabled>Choose folder & render</button><button id="movieClose">Close</button></div>`;
  document.body.append(dialog);
  const element = <T extends HTMLElement>(id: string): T => dialog.querySelector(`#${id}`) as T;
  const events = element<HTMLSelectElement>("movieEvent");
  for (const name of renderer.events) events.add(new Option(name, name));
  events.value = renderer.currentEvent() ?? "GW150914";
  const settings = element<HTMLFieldSetElement>("movieSettings");
  const start = element<HTMLButtonElement>("movieStart");
  const close = element<HTMLButtonElement>("movieClose");
  const reveal = element<HTMLButtonElement>("movieReveal");
  const status = element<HTMLParagraphElement>("movieStatus");
  const progress = element<HTMLProgressElement>("movieProgress");
  const format = element<HTMLSelectElement>("movieFormat");
  const bridge = window.kerrMovie;
  let running = false, cancelled = false, preparing = false;
  const options = (): MovieOptions => {
    const [width, height] = element<HTMLSelectElement>("movieResolution").value.split("x").map(Number);
    return { event: events.value, width: width!, height: height!,
      fps: Number(element<HTMLSelectElement>("movieFps").value),
      duration: Number(element<HTMLInputElement>("movieDuration").value), format: format.value as "mp4" | "png" };
  };
  const estimate = (): void => {
    const value = options();
    const count = value.fps * value.duration;
    const gib = value.width * value.height * 4 * count * 1.2 / 1024 ** 3 + 0.25;
    element<HTMLParagraphElement>("movieEstimate").textContent =
      `${count.toLocaleString()} frames · allow up to ${gib.toFixed(1)} GiB free for the retained PNG sequence and movie. Actual size depends on the image.`;
  };
  settings.addEventListener("input", estimate);
  estimate();
  const requestCancel = (): void => {
    if (preparing) return;
    if (!running) { dialog.close(); return; }
    cancelled = true;
    close.disabled = true;
    status.textContent = "Cancelling… completed PNG frames will be kept.";
    void bridge?.cancel().catch(error => { status.textContent = String(error); });
  };
  close.addEventListener("click", requestCancel);
  dialog.addEventListener("cancel", event => { event.preventDefault(); requestCancel(); });
  // Keep H/Escape and camera shortcuts from changing the scene under a modal export.
  dialog.addEventListener("keydown", event => event.stopPropagation());
  dialog.addEventListener("close", () => dialog.remove());
  reveal.addEventListener("click", () => {
    void bridge?.reveal().then(error => { if (error) status.textContent = error; });
  });
  start.addEventListener("click", () => { void run(); });

  async function run(): Promise<void> {
    if (!bridge || running || !element<HTMLInputElement>("movieDuration").reportValidity()) return;
    running = true; cancelled = false; preparing = true;
    settings.disabled = true; start.disabled = true; close.disabled = true;
    reveal.hidden = true; progress.value = 0;
    let begun = false, folder: string | null = null;
    try {
      const config = options();
      const metadata = renderer.begin(config);
      begun = true;
      status.textContent = "Choose an output folder. A new export folder will be created inside it.";
      const job = await bridge.start(config, metadata);
      preparing = false;
      if (!job) { status.textContent = "Folder selection cancelled."; return; }
      folder = job.folder;
      reveal.hidden = false;
      close.disabled = false; close.textContent = "Cancel export";
      progress.max = job.frameCount;
      const started = performance.now();
      for (let index = 0; index < job.frameCount; index++) {
        if (cancelled) break;
        status.textContent = `Rendering frame ${index + 1} of ${job.frameCount}…`;
        // Yield for paint/cancel without tying the movie clock to RAF or wall time.
        await new Promise(resolve => setTimeout(resolve, 0));
        if (cancelled) break;
        const png = await renderer.render(index);
        if (cancelled) break;
        await bridge.append(index, png);
        progress.value = index + 1;
        const elapsed = (performance.now() - started) / 1000;
        const remaining = elapsed / (index + 1) * (job.frameCount - index - 1);
        status.textContent = `${index + 1} / ${job.frameCount} frames saved · about ${Math.ceil(remaining / 60)} min remaining for frames`;
      }
      if (cancelled) {
        await bridge.cancel();
        status.textContent = `Cancelled. Completed frames kept in ${folder}`;
      } else {
        status.textContent = config.format === "mp4" ? "Encoding and verifying MP4… PNG frames are safely saved." : "Finishing PNG sequence…";
        progress.removeAttribute("value");
        const result = await bridge.finish();
        progress.value = progress.max;
        status.textContent = `Saved ${config.duration}s at ${config.fps} FPS: ${result.movie ?? result.folder}`;
      }
    } catch (error) {
      if (folder) await bridge.cancel().catch(() => {});
      status.textContent = (cancelled ? "Cancelled." : `Export failed: ${error instanceof Error ? error.message : String(error)}`) +
        (folder ? ` Completed frames are in ${folder}` : "");
    } finally {
      if (begun) renderer.end();
      running = false; preparing = false;
      settings.disabled = false; start.disabled = false; close.disabled = false; close.textContent = "Close";
      if (!progress.hasAttribute("value")) progress.value = 0;
    }
  }

  dialog.showModal();
  if (!bridge) {
    status.textContent = "Movie export is available in the KerrLab desktop app. Open the desktop build to save a clip.";
    return;
  }
  void bridge.capabilities().then(capabilities => {
    if (!dialog.isConnected) return;
    if (!capabilities.mp4) {
      format.value = "png";
      format.options[0]!.disabled = true;
      status.textContent = `${capabilities.reason} PNG export is available.`;
    } else status.textContent = "Ready. Adjust the scene before opening this dialog to use your preferred camera and effects.";
    start.disabled = false;
  }).catch(error => { status.textContent = `Could not check encoder: ${String(error)}`; });
}
