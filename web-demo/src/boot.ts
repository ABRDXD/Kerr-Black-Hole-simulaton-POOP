// KerrLab wrapper: make startup failures visible before the renderer loads.
export {};
const status = document.createElement("div");
status.id = "render-status";
status.setAttribute("role", "status");
Object.assign(status.style, {
  position: "fixed", inset: "0", zIndex: "100", display: "grid",
  placeContent: "center", padding: "32px", background: "#06080d",
  color: "#e7ebf5", font: "15px/1.6 system-ui, sans-serif",
});
const heading = document.createElement("h1");
heading.textContent = "Opening KerrLab";
const detail = document.createElement("p");
detail.textContent = "Preparing the black hole and its light paths…";
const message = document.createElement("pre");
Object.assign(message.style, { whiteSpace: "pre-wrap", maxWidth: "min(760px, 85vw)", maxHeight: "40vh", overflow: "auto", color: "#f5c487" });
const retry = document.createElement("button");
retry.textContent = "Reload renderer";
retry.hidden = true;
retry.addEventListener("click", () => location.reload());
status.append(heading, detail, message, retry);
document.body.append(status);
let failed = false;

function showFailure(reason: unknown): void {
  failed = true;
  heading.textContent = "The renderer could not continue";
  detail.textContent = "Reload to try again. If this persists, check that your graphics driver supports WebGL 2 and hardware acceleration. On Arch, run the included graphics diagnostic: bash tools/doctor-arch.sh.";
  message.textContent = reason instanceof Error ? reason.message : String(reason);
  console.error("KerrLab renderer failure:", message.textContent);
  retry.hidden = false;
  status.setAttribute("role", "alert");
  if (!status.isConnected) document.body.append(status);
}

window.addEventListener("error", (event) => showFailure(event.error ?? event.message));
window.addEventListener("unhandledrejection", (event) => showFailure(event.reason));
const canvas = document.getElementById("view");
canvas?.addEventListener("webglcontextlost", (event) => {
  event.preventDefault();
  showFailure("The graphics context was lost. Reload to recreate the renderer.");
});

// Give the loading message one paint before synchronous shader compilation.
requestAnimationFrame(() => {
  void import("./main").then(() => {
    requestAnimationFrame(() => { if (!failed) status.remove(); });
  }).catch(showFailure);
});
