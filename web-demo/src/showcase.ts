// KerrLab presentation layer. Presets change only the existing scene inputs;
// geodesics, disk shading, and numerical integration remain upstream code.
import type { OrbitCamera } from "./camera";
import type { PanelParams } from "./panel";
import "./showcase.css";

interface ShowcaseOptions {
  params: PanelParams;
  camera: OrbitCamera;
  resetSingleHole(): void;
  refreshPanel(): void;
  screenshot(): void;
  movie(): void;
}

interface Scene {
  name: string;
  caption: string;
  apply(p: PanelParams, c: OrbitCamera): void;
}

const scenes: readonly Scene[] = [
  {
    name: "Event horizon",
    caption: "A luminous disk. A light-trapping shadow.",
    apply: () => {},
  },
  {
    name: "Gargantua",
    caption: "An edge-on encounter with a rapidly spinning black hole.",
    apply: (p, c) => {
      p.spin = 0.92;
      p.diskGain = 10;
      p.bloomStrength = 0.45;
      c.radius = 15;
      c.elevation = 0.065;
    },
  },
  {
    name: "Polar orbit",
    caption: "Above the disk, against a lensed Milky Way.",
    apply: (p, c) => {
      p.spin = 0.8;
      p.diskOuter = 14;
      p.diskGain = 8;
      p.bloomStrength = 0.22;
      p.skyRich = true;
      c.azimuth = 0.45;
      c.elevation = 1.12;
      c.radius = 20;
    },
  },
  {
    name: "Extreme Kerr",
    caption: "Near-extremal spin. A closer view of the inner disk.",
    apply: (p, c) => {
      p.spin = 0.998;
      p.diskOuter = 10;
      p.diskGain = 7;
      p.bloomStrength = 0.3;
      c.radius = 12;
      c.elevation = 0.2;
    },
  },
];

function button(label: string, title: string, action: () => void): HTMLButtonElement {
  const el = document.createElement("button");
  el.type = "button";
  el.textContent = label;
  el.title = title;
  el.addEventListener("click", action);
  return el;
}

export function buildShowcase(options: ShowcaseOptions): void {
  const { params, camera, resetSingleHole, refreshPanel, screenshot } = options;
  const header = document.createElement("header");
  header.id = "showcaseHeader";
  header.innerHTML = `<div class="observatory-label"><span class="orbit-mark" aria-hidden="true"></span>BLACK HOLE OBSERVATORY</div>
    <h1>Kerr<span>Lab</span></h1>
    <p id="sceneCaption"></p>
    <div class="showcase-meta">LIVE GEODESIC RAY TRACING <span>·</span> ARTISTIC DISK &amp; SKY</div>`;
  const caption = header.querySelector("#sceneCaption") as HTMLParagraphElement;
  caption.textContent = scenes[0]!.caption;
  document.body.append(header);

  const dock = document.createElement("nav");
  dock.id = "sceneDock";
  dock.setAttribute("aria-label", "Black hole scenes and view controls");
  const sceneRow = document.createElement("div");
  sceneRow.className = "scene-buttons";
  const sceneButtons: HTMLButtonElement[] = [];
  const choose = (scene: Scene, index: number): void => {
    resetSingleHole();
    scene.apply(params, camera);
    refreshPanel();
    sceneButtons.forEach((el, i) => el.setAttribute("aria-pressed", String(i === index)));
    caption.textContent = scene.caption;
  };
  scenes.forEach((scene, i) => {
    const el = button(scene.name, scene.caption, () => choose(scene, i));
    el.setAttribute("aria-pressed", String(i === 0));
    sceneButtons.push(el);
    sceneRow.append(el);
  });
  const actions = document.createElement("div");
  actions.className = "showcase-actions";
  const hint = document.createElement("span");
  hint.className = "interaction-hint";
  hint.textContent = "Drag to orbit · Scroll to explore";

  const reveal = button("Show interface · H", "Restore interface (H or Escape)", () => setCinema(false));
  reveal.id = "restoreInterface";
  reveal.hidden = true;
  const cinema = button("Cinema view", "Hide interface (H); Escape restores it", () => setCinema(true));
  cinema.setAttribute("aria-pressed", "false");
  const setCinema = (hidden: boolean): void => {
    document.body.classList.toggle("cinema-view", hidden);
    reveal.hidden = !hidden;
    cinema.setAttribute("aria-pressed", String(hidden));
    if (hidden) reveal.focus();
    else cinema.focus();
  };
  const fullscreen = button("Fullscreen", "Toggle fullscreen", () => {
    const request = document.fullscreenElement
      ? document.exitFullscreen()
      : document.documentElement.requestFullscreen?.();
    void request?.catch(() => {
      fullscreen.textContent = "Fullscreen unavailable";
    });
  });
  document.addEventListener("fullscreenchange", () => {
    fullscreen.textContent = document.fullscreenElement ? "Exit fullscreen" : "Fullscreen";
  });
  actions.append(
    hint,
    button("Reset", "Restore the default single Kerr scene", () => choose(scenes[0]!, 0)),
    button("Save PNG", "Save this rendered view without interface", screenshot),
    button("Render movie", "Export a merger clip at 24, 30, 60 or 144 FPS", options.movie),
    fullscreen,
    cinema,
  );
  dock.append(sceneRow, actions);
  document.body.append(dock, reveal);

  document.addEventListener("keydown", (event) => {
    const target = event.target;
    if (event.ctrlKey || event.metaKey || event.altKey ||
        target instanceof HTMLElement &&
        (target.isContentEditable || /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName))) return;
    if (event.key.toLowerCase() === "h") {
      event.preventDefault();
      setCinema(!document.body.classList.contains("cinema-view"));
    } else if (event.key === "Escape" && document.body.classList.contains("cinema-view")) {
      setCinema(false);
    }
  });

  // Do not leave a named preset selected after a manual scientific edit.
  const markCustom = (): void => {
    sceneButtons.forEach((el) => el.setAttribute("aria-pressed", "false"));
    caption.textContent = "Your view of curved spacetime.";
  };
  const panel = document.getElementById("panel");
  // Keep the rendered scene visible on narrow displays; the existing panel
  // tab remains available for every scientific control.
  if (matchMedia("(max-width: 650px)").matches) {
    panel?.classList.add("hidden");
    const toggle = document.getElementById("panelToggle");
    if (toggle) toggle.textContent = "⟨";
  }
  panel?.addEventListener("input", markCustom);
  panel?.addEventListener("change", markCustom);
  panel?.addEventListener("click", (event) => {
    if (event.target instanceof HTMLElement && event.target.closest("button.preset")) markCustom();
  });
}
