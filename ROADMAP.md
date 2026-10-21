# KerrLab: product roadmap and continuation guide

Last reviewed: **2026-10-21**. This is the primary future-work document.

User-directed movie export slice (2026-10-21): **Render movie** now supports a
12-second full merger at 24/30/60/144 FPS, fixed resolution through 4K, recoverable
PNG sequences and external FFmpeg MP4 encoding/verification. This advances a
bounded D6 slice ahead of the default order at the user's explicit request.
43 local tests pass, including real 12-second encoder checks at 60/144 FPS with
generated images. Graphical acceptance remains open because this restricted
session cannot start Electron's sandbox or access GPU devices. See
[MOVIE-EXPORT.md](MOVIE-EXPORT.md) and the
[implementation record](benchmarks/results/2026-10-21-movie-export.md).

Linux restoration update: the working local folder is now
`/home/abrsdxd/Projects/KerrLab-Restored`. Desktop launch and visible rendering,
offline rebuild/package, 15 quality tests and nine native CPU suites passed on
Arch. The app menu entry and `kerrlab` terminal command launch this folder.
See `RUN-ME.md` and `benchmarks/results/2026-10-14-linux-restore.md` for current
evidence, remaining checks and the next step. Historical Windows paths below
describe the previous host.

Diagnostics continuation (2026-10-14): corrected Electron error logging and load
failures, separated visible FPS measurements from captures, and implemented a
packaged desktop check/measurement command. All 28 local regression tests and the
offline build/package passed. A fresh graphical run was blocked before JavaScript
by the active session's GPU/sandbox restrictions, so desktop acceptance remains
open. See `benchmarks/results/2026-10-14-desktop-diagnostics.md` and
`web-demo/e2e/DESKTOP-CHECKS.md` for evidence and the next runnable check.

Read this file and `AGENTS.md` before continuing the project. `IMPLEMENTATION_PLAN.md`
retains the detailed native-engine milestone history; it is not the current desktop
product backlog. `web-demo/PROJECT_PLAN.md` belongs to the imported upstream project,
not to KerrLab's local delivery plan. User instructions take precedence over this roadmap.

## 1. User intent and decisions already made

- Deliver a visually impressive, interactive black-hole application **in its own
  Windows window**. A browser tab is a development option, not the default experience.
- The user submitted another version and removed the earlier immediate deadline.
  They want substantial further development rather than only a minimal render.
- The user explicitly requested extensive reuse of `kyleyhw/black_hole`. Its complete
  pinned tree was imported locally. Preserve that work and attribution; do not repeatedly
  ask whether local reuse is wanted. See `THIRD_PARTY.md` for the unresolved upstream
  license status; local import permission is not a public redistribution license.
- Keep the working desktop app usable while developing deeper physics. A Vulkan
  rewrite is not a prerequisite for adding useful desktop interaction.
- Preserve the independently implemented, validated C++ core. Its test results do
  not automatically validate the imported TypeScript, GLSL or WGSL implementations.
- Artistic presentation is welcome. Distinguish it from a physical model and state
  the approximations in the relevant help, scientific views and exported metadata.
- This roadmap is a plan, not authorization to publish, buy services, create accounts,
  or run background automations. Routine local implementation and verification should
  proceed within the user's active task without repeated permission requests.

## 2. Starting point: what exists now

Status vocabulary: **verified** means supported by local evidence; **implemented**
means code exists but its full behavior has not been accepted; **partial** means only
part of a planned system exists; **planned** means no accepted implementation exists.

| Area | Current status and evidence boundary |
|---|---|
| Standalone desktop window | Verified basic launch and visible black-hole render. Electron 44.7.0 loads packaged files directly. No web server or separate Node installation is needed to run the packaged app. |
| Live image | Imported WebGL 2 Kerr tracing, disk, procedural stars, HDR/bloom, manual quality and diagnostic controls. Visual inspection establishes a working image, not scientific accuracy across its entire domain. |
| Local presentation | Four showcase scenes, Reset, Cinema view, fullscreen and startup/context-loss errors implemented. Several scene changes were inspected in the browser; full desktop interaction coverage remains open. |
| Cameras | Orbit controls and imported basic free-fall exist. Independent free flight/WASD and native observer-camera integration are planned. |
| Merger/multi-mass | Imported approximate scenes and GW event animation exist. A merger animation was observed in the browser. These are not numerical-relativity solutions or a general N-body solver. |
| PNG capture | Preview, download link and Electron save-dialog handling implemented. End-to-end native save/cancel verification is still open. The prior UI check stopped on the user's physical Escape key. |
| HQ stills | Optional separate WebGPU progressive renderer implemented upstream. Desktop acceptance and parity with live effects are open; rich sky/bloom are not fully represented in its state. |
| C++ physics | Geometry, adaptive null/timelike tracing, observers, clocks, frame transport and bounded observer sessions implemented with CLI CSV tools. Historical 2026-10-07 record: 95,463 CPU checks in nine suites. |
| Native Vulkan | Metric/derivative conformance and a gradient/checkerboard window diagnostic implemented. Historical record: 723 fixtures, 58,302 floating comparisons, 27 ABI rejection cases. No native black-hole image yet. |
| Native window reliability | Both local GPUs completed lifecycle checks, but intermittent AMD restore/suspension timeouts remain unexplained. Electron visual success does not close this GLFW/Vulkan issue. |
| Movie export | Implemented fixed-camera full-merger export, fixed full resolution, 24/30/60/144 FPS, PNG sequence and verified MP4 writer. Local encoder/timeline tests pass; actual GPU/dialog acceptance remains open. |
| Product persistence/experiments | User scene save/load, particles, observer-clock UI and camera paths are planned. |
| Packaging | Local portable folder exists; no finished installer, signing, updater or public-release process. Packaging stages fresh output before replacing the previous folder. Target acceptance and build identification work remain open. |

The desktop showed about 31–33 fps during one prior visual check. This is **not a
benchmark, a minimum, or a 60-fps claim**. Imported reports under `web-demo/e2e`,
`bench` and `validation` are upstream artifacts, not tests rerun on this machine.
See `benchmarks/results/2026-10-12-desktop.md` for the retrospective evidence record.

## 3. Navigation and commands for the next session

Workspace: `C:\Users\Abhinav\kerr-simulator`. Check actual files before assuming the
last recorded state remains current. Git is now installed, but no `.git` directory
was present in this project at this review. Do not claim a commit or PR exists.

| Location | Purpose |
|---|---|
| `start-black-hole.cmd`, `start-black-hole.ps1` | Default desktop launcher and build helper; `-Browser` explicitly selects optional web preview. |
| `out/desktop/KerrLab/KerrLab.exe` | Current generated portable app; keep its surrounding files intact. Not source. |
| `web-demo/desktop/main.cjs` | Electron window, local file loading, permissions/navigation restrictions, download dialog, diagnostics. |
| `tools/package-desktop.cjs` | Local portable assembly and packaged content policy. |
| `web-demo/src/boot.ts` | Loading/error presentation and context-loss detection. |
| `web-demo/src/main.ts` | Current renderer orchestration, frame timing, merger state, capture and reset. |
| `web-demo/src/camera.ts`, `panel.ts`, `showcase.ts`, `showcase.css` | Camera, upstream controls and local cinematic interface. |
| `web-demo/src/tetrad.ts`, `geodesic.ts`, `physics.ts`, `merger.ts` | Imported physics/approximation implementations. |
| `web-demo/src/shaders/`, `web-demo/src/webgpu.ts` | Live GLSL and separate HQ WGSL paths. |
| `include/kerr/physics/`, `src/physics/`, `include/kerr/numerics/` | Native reference physics and integrators. |
| `include/kerr/app/ObserverSession.hpp`, `src/app/ObserverSession.cpp` | Native session ownership, generations, clocks and histories. |
| `src/app/*_main.cpp`, `tests/`, `GPU_PHYSICS.md` | Existing CLI contracts, numerical tests and packed GPU specification. |
| `web-demo/UPSTREAM.md`, `UPSTREAM_FILES.json`, `THIRD_PARTY.md` | Imported revision, original file hashes, local modifications and notices. |

Commands from the project root:

```powershell
# Launch the existing desktop build.
.\start-black-hole.ps1

# Close the running KerrLab app first. Build/package without opening a window.
.\start-black-hole.ps1 -Build -NoOpen

# Explicit optional browser development preview, not the normal launch route.
.\start-black-hole.ps1 -Browser -Build -NoOpen -Port 4173

# Native reference checks when native code changes.
.\build.ps1 -Configuration Release
.\build.ps1 -Configuration Debug
.\build.ps1 -Configuration Sanitize

# Native Vulkan/window checks when that subsystem changes.
.\build.ps1 -Configuration Release -Window -RequireValidation
.\tools\validate-vulkan.ps1 -Window -Device 1
```

`-NoOpen` in desktop mode builds/checks availability without launching; it does
not start a web server. Device indices are local: probe before using them elsewhere.
The launcher can use installed Node or the existing bundled runtime. First builds
restore dependencies and the pinned Electron runtime; ordinary packaged launches
are offline. Within `web-demo`, `npm run build` checks TypeScript and bundles assets;
`npm run desktop` runs the Electron shell after building; `npm run desktop:package`
builds and assembles the portable folder. `npm ci --ignore-scripts` alone does not
install Electron's executable; the launcher explicitly invokes its installer.

## 4. Default order and release boundaries

Start with **D0**, then **D1 → D2 → D3 → D4 → D5**. D6 can branch after D1/D2;
D7 after D3; D8 starts with baseline measurement in D0 and expands once D1 exists.
N1–N4 are a parallel native-rendering track; do not let that longer work prevent
shipping desktop improvements. D9 contains optional visual extensions. R1 packages
an explicitly chosen, verified release scope.

| ID | Outcome | Dependencies | Relative size |
|---|---|---|---|
| D0 | Reliable, documented desktop baseline | Current app | Small–medium |
| D1 | Scene/session model, settings and save/load | D0 | Medium |
| D2 | Free-flight exploration camera | D1 | Medium |
| D3 | Defined native data contract and bounded physics bridge | D0/D1 | Large |
| D4 | Interactive probes, trajectories and trails | D2/D3 | Large |
| D5 | Observer clocks and physical camera experiments | D3/D4 | Large |
| D6 | Reproducible stills, camera paths and animation export | D1/D2; D4/D5 for experiments | Large |
| D7 | Ray inspector and scientific diagnostics | D3; N1/N2 for native GPU comparisons | Large |
| D8 | Measured quality controller and stable rendering | D0/D1; repeat measurements as workloads change | Medium–large |
| D9 | Richer disk, environments and optional effects | Stable baseline; N3 for reference disk physics | Large/open-ended |
| N1 | Native GPU geodesic integration and failure classes | Existing conformance + D3 conventions | Large |
| N2 | Validated native sky/shadow/lensing images | N1 + native window compatibility | Large |
| N3 | Native radiative disk and spectral reference | N2 | Large |
| N4 | Product integration of the native image renderer | N2/N3 + measured presentation spike | Large |
| R1 | Reproducible desktop release and user guide | Chosen feature set accepted | Medium |

Sizes express risk/complexity, not calendar promises. Re-estimate after each phase's
first working slice. Do not present an arbitrary percentage or date as measured progress.

- **Visible desktop milestone:** achieved for basic launch/render; D0 closes reliability gaps.
- **Explorer release:** D0–D6 plus measured D8 basics and R1. Rich visual extras can ship later.
- **Scientific release:** additionally D7 and N1–N4, with documented domain/tolerance gates.
- Full plasma/MHD simulation, general dynamical spacetimes and numerical-relativity
  binary evolution are separate research projects, not hidden requirements for Explorer.

## 5. Desktop feature backlog

### D0 — Stabilize the deliverable first

Goal: someone can open, use, close and relaunch the app without a developer present.

- [x] D0.1 Build the desktop shell and verify an actual black-hole image in its window.
- [x] D0.2 Correct the future-work entry documents and record prior evidence/limitations.
- [ ] D0.3 Finish PNG checks: preview is nonblank, dimensions match, save succeeds,
  saved PNG decodes, cancellation is clean, and repeated capture releases resources.
- [ ] D0.4 Make startup/export help appropriate to desktop; remove instructions that
  tell a desktop user to open browser menus. Verify Electron log/error handlers against
  the pinned version. Make load failures actionable rather than a silent blank window.
  Console/load handling and renderer error surfacing are implemented with local
  regression coverage (2026-10-14); native error-dialog acceptance remains open.
- [ ] D0.5 Test all four scenes; reset from active merger, free-fall, overlays and
  nondefault quality; check original sidebar presets, H/Escape and F11.
- [ ] D0.6 Check resize, minimize/restore, display scale changes, close/relaunch,
  single-instance focus, missing/corrupt assets and graphics-context failure recovery.
- [ ] D0.7 Establish reproducible desktop smoke checks; adapt the imported harness
  rather than assuming its Linux Chromium path or undeclared Playwright works here.
  Local Electron runner using existing dependencies implemented at `tools/check-desktop.cjs`;
  syntax/help checked, graphical execution pending on a normal desktop session.
- [ ] D0.8 Package into a fresh staging/versioned output folder, verify assets/notices,
  then replace the output only after checks. Avoid stale hashed bundles and partial
  overwrite of a running executable. Add an identifiable application version/build ID.
- [ ] D0.9 Record a baseline at fixed dimensions, scene, settings, device, driver and
  power state. Include warmup, frame-time percentiles, memory and failure logs.

Acceptance: launch and real image verified; capture/save/cancel verified; documented
window/scene checks pass; generated package runs without a dev server; no unexplained
new runtime errors. HQ stills may be separately marked experimental if not accepted.
Relevant files: desktop shell, launcher, packager, boot, main, showcase and e2e harness.

### D1 — Own the scene state and make it persistent

Goal: every subsequent feature uses one documented scene/session model.

- [ ] D1.1 Split responsibilities out of `main.ts` incrementally: scene state, clock,
  camera controller, capture, render inputs and UI commands. Preserve a working image
  after each slice; a wholesale framework rewrite is not a prerequisite.
- [ ] D1.2 Define a versioned, validated scene schema containing units/model ID,
  dimensionless spin, physical mass scale, camera mode/pose, disk/environment,
  presentation quality, simulation/export time, deterministic seeds and presets.
- [ ] D1.3 Keep live GPU resources, native handles, pending jobs and transient UI out
  of serialized files. Distinguish restartable scene configuration from a full dynamic
  checkpoint. Do not promise checkpoint resume until integrator/session state is covered.
- [ ] D1.4 Route parameter changes through commands. Separate display edits from
  spacetime changes; physical mass/spin changes create a new session generation and
  invalidate observers, trajectories and accumulated images consistently.
- [ ] D1.5 Add New/Open/Save/Save As, custom presets, import/export, remembered quality
  and window bounds, and a reset-to-defaults action. Use a narrow desktop file API.
- [ ] D1.6 Validate finite values, bounds, version and array/file sizes; reject malformed
  files without losing the active scene. Preserve the old file on failed writes.

Acceptance: round-trip every supported mode; unknown versions and invalid inputs give
useful errors; fresh launch restores settings; identical seeded scene inputs reproduce
the same state. GPU comparisons use declared tolerances, not cross-device bit equality.

### D2 — Explore with a proper free-flight camera

- [ ] D2.1 Add explicit Orbit and Explore modes; retain orbit and reset. Reserve a
  Physical observer mode in the state/API, but do not offer it as working until D5.
  Explore supports independent position/orientation, WASD, vertical movement,
  mouse-look and speed adjustment. Input help and focus behavior must be visible.
- [ ] D2.2 Prevent movement while typing in controls or using modal dialogs; clear held
  keys on focus loss; release pointer capture on Escape; handle frame stalls safely.
- [ ] D2.3 Define near-hole limits for Explore. A sequence of static cameras can cease
  to be physical near the ergoregion. Use a valid local observer frame or explicitly
  constrain the mode; do not hide invalid tetrads behind a cosmetic camera clamp.
- [ ] D2.4 Add smooth reset/transitions and save camera pose/mode through D1. Label
  cinematic movement accurately; it is not automatically a freely falling worldline.

Acceptance: movement is predictable across frame rates; transitions/reset cannot leave
stuck input or NaNs; selected known poses match ray-basis fixtures; camera controls stay
usable after dialogs/minimize/restore. Defer physical observer motion to D5.

### D3 — Bridge the existing C++ science into the app

Goal: reuse reference physics without exposing arbitrary process or filesystem access.

- [ ] D3.1 Write the convention/serialization contract listed in section 7 and explicit
  adapters. Validate fixtures before integrating a live worker.
- [ ] D3.2 First vertical slice: import one bounded, versioned trajectory exported by
  the existing CLI and show its metadata/termination state. Keep existing CSV tools
  useful; do not silently reinterpret their coordinate/time conventions.
- [ ] D3.3 Add a local fixed-purpose C++ worker if live requests are needed. Prefer
  bounded structured messages over shell command strings. Native process lifecycle
  belongs to Electron main; a narrow preload/API keeps the renderer sandboxed.
- [ ] D3.4 Requests carry schema/model version, session generation, job ID, finite
  validated parameters and work/history limits. Support cancellation, stale-result
  rejection, worker failure/restart and bounded response queues.
- [ ] D3.5 Distribute the worker and its required runtime libraries/notices in the
  local package; verify startup independent of development PATH and working directory.

Acceptance: agreed fixtures round-trip without convention loss; invalid messages fail
explicitly; changing spin during a job cannot attach old results to a new scene; heavy
tracing does not freeze the UI; no Node/process/filesystem escape is exposed to page code.

### D4 — Probes, particles and visible trajectories

- [ ] D4.1 Spawn/select/delete a bounded number of probes with validated position and
  local velocity; include circular-orbit and infall examples using native worldlines.
- [ ] D4.2 Add trails, names, velocity markers, termination/failure labels and a common
  simulation clock with pause, bounded single-step, speed and reset.
- [ ] D4.3 Bound history/memory; decimate for display without changing the integrated
  trajectory; show dropped-history/work-limit status. Save initial conditions via D1.
- [ ] D4.4 Initially label trails as **coordinate-space explanatory overlays**. Drawing
  projected points over the image does not make their light correctly lensed or delayed.
  Physically observed moving objects need light-cone intersection and visibility work.

Acceptance: display follows native snapshots; analytic orbit/infall cases retain their
numerical checks; high requested counts stay bounded; pause/step/reset are consistent;
failed/captured probes cannot continue producing invalid samples. No N-body claim.

### D5 — Clocks, observers and time-dilation experiments

- [ ] D5.1 Expose stationary, circular and free-falling observers with individual proper
  time and a clearly named common Kerr–Schild coordinate-time axis.
- [ ] D5.2 Plot clock differences, radius and selected invariants from bounded histories;
  provide near/far and orbit/infall examples with plain-language explanation.
- [ ] D5.3 Switch the image camera to a native observer event and transported tetrad,
  using D3 adapters and verifying ray direction, aberration and frequency conventions.
- [ ] D5.4 Reject unavailable stationary/orbital states; show explicit horizon/event
  termination and solver failure. Reset clears observers by generation.
- [ ] D5.5 Add exchanged-light/received-clock experiments only as a separate later slice:
  synchronized coordinate snapshots are not what a distant observer optically receives.

Acceptance: reproduce native stationary/circular/infall clock fixtures; special-
relativistic limits and tetrad null-ray tests pass; graph labels and exported units are
unambiguous. Never imply the imported basic free-fall already meets these guarantees.

### D6 — Capture, camera paths and reproducible movies

2026-10-21: the requested merger-only vertical slice implements D6.3/D6.4 and
encoding aspects of D6.5, with dialog/progress/cancellation, exact frame timing,
full-resolution rendering and restored preview state. MP4 uses existing external
FFmpeg/libx264/ffprobe, records their actual versions and bundles no encoder.
This is an explicit external-runtime choice rather than the planned pinned bundle.
Native movie verification: `node tools/check-desktop.cjs --movie`. That run still
needs a normal desktop session. No camera paths, motion blur, generic scene
serialization, audio or automatic interrupted-render resume is claimed. Leave
the broader D6 gates open until the scope below and target acceptance are met.

- [ ] D6.1 Complete reliable still capture and metadata first; define which live effects
  are included. Either align HQ/live effects or clearly describe their differences.
- [ ] D6.2 Add camera keyframes, interpolation, duration, easing and preview/scrubbing;
  handle orientation without Euler-angle discontinuities or unintended near-hole paths.
- [ ] D6.3 Use a deterministic export clock independent of wall-clock FPS. Freeze seeds,
  quality and exposure; define how animated disks, probes and mergers advance.
- [ ] D6.4 Export PNG sequences before adding video encoding. Show frame/progress/error
  status, cancellation, disk-space estimates, bounded memory and safe output naming.
- [ ] D6.5 Add video encoding through a pinned, licensed dependency only when needed;
  expose resolution/frame rate/duration and verify the output decodes with correct timing.

Acceptance: repeated export from the same configuration produces equivalent frames under
declared tolerances; no missing/duplicated frame times; cancellation leaves recoverable
output; exports exclude UI unless requested. High resolution respects GPU/memory limits.

### D7 — Explain individual rays and numerical confidence

- [ ] D7.1 Click a pixel to generate a bounded native reference ray using the exact saved
  camera/scene conventions. Show path, disk crossings, escape/capture and solver status.
  Initially support single-Kerr scenes only: the native core has no corresponding
  binary or weak-field multi-mass model. Reject unsupported model comparisons visibly;
  never present a single-Kerr trajectory as the reference for a merger image.
- [ ] D7.2 Add linked scene/trajectory views and plots for affine parameter, redshift,
  step count, constraint residual and conserved quantities with explicit units/model.
- [ ] D7.3 Give invalid, budget-exhausted, under-resolved, captured and escaped rays
  distinct diagnostic classes. Audit upstream black output/failure classifications;
  a dark pixel is not sufficient evidence of horizon capture.
- [ ] D7.4 Compare imported and native rays/images through declared fixtures. Existing
  overlays remain useful, but do not rename them a validated scientific renderer.

Acceptance: selected-pixel ray agrees with the configured frame and reference fixtures;
failures remain inspectable; no synchronous full-frame readback is added to normal live
frames. CPU tracing jobs are cancellable and do not block camera interaction.

### D8 — Performance without silently changing the science

- [ ] D8.1 Instrument actual frame/available GPU timings, internal dimensions, DPR,
  ray budget, memory and mode. Separate warmup/compilation from steady-state samples.
  Visible RAF history and diagnostic snapshots implemented; desktop runner supports
  warmup, raw intervals, percentiles, dimensions, GPU identity and process metrics.
  Real target measurements and GPU execution timing remain open.
- [ ] D8.2 Add named quality profiles and a target-FPS controller with hysteresis,
  cooldown and explicit floor/ceiling. Start by changing visual resolution only.
- [ ] D8.3 Keep numerical tolerance/work-budget controls separate; reference mode locks
  them. Report unmet targets honestly rather than falsely claiming 30/60/120 fps.
- [ ] D8.4 Reduce startup stalls and unused work based on profiling, including optional
  shader compilation and render-target sizing. Preserve error reporting and correctness.
- [ ] D8.5 Investigate star aliasing/temporal stability with camera, disk and observer
  motion. Any history accumulation must invalidate on scene generation and discontinuity.
- [ ] D8.6 Treat DLSS as an optional native-renderer extension after N4, with correct
  motion/depth semantics, a fallback and applicable dependency/license review.

Acceptance: repeatable frame-time/memory measurements on the intended laptop; no quality
oscillation; scientific buffers/accuracy remain within their stated gates; moving scenes
do not retain false shadows or ghosted disk features. Targets depend on workload/hardware.

### D9 — Visual and educational extensions (optional slices)

- [ ] Finite-thickness/volumetric disk with documented absorption/emission approximation;
  keep a thin-disk reference mode and compare against N3 where applicable.
- [ ] Better filtered stars, environment selection/import, exposure controls and color
  presentation. Retain asset provenance and deterministic reference environments.
- [ ] Hotspots, artistic jets and emissive structures only with an explicit model label;
  magnetic plasma/jet dynamics require new physics rather than a particle animation claim.
- [ ] Merger timeline/scrubbing and optional waveform sonification; a converted signal
  is not literal sound propagating through empty space. Preserve data attribution.
- [ ] Guided experiments, contextual lessons, bookmarks and camera tours using accepted
  features. Avoid replacing a functioning viewport with a large onboarding flow.

Acceptance is per slice: a controllable feature, preserved baseline, bounded workload,
tested transitions/export, and honest labels. These extras do not block the core release.

## 6. Native image-rendering track

Read `PHYSICS.md`, `NUMERICS.md`, `VALIDATION.md`, `GPU_PHYSICS.md`, `RENDERING.md`
and the native sections of `IMPLEMENTATION_PLAN.md` before numerical/shader work.

### N1 — GPU trajectories beyond pointwise geometry

- [ ] Extend packed fixtures from metrics/derivatives to Hamiltonian RHS and individual
  integration steps, then bounded complete null trajectories.
- [ ] Define error/work budgets, event localization, positive/negative spin and extremal
  behavior; distinguish capture, escape, disk hits, invalid states and exhausted work.
- [ ] Validate against the CPU reference plus independent analytic/converged cases.
  Geometry tolerances are not trajectory tolerances; agree criteria before benchmarking.
- [ ] Keep mass/coordinate scaling, future/past ray conventions and ABI versions explicit.

Acceptance: finite outputs or typed failures, bounded work, invariant/convergence reports,
event/classification fixtures and passing ABI checks on available target GPUs.

### N2 — Native sky, shadow and lensing

- [ ] Investigate the native AMD restore/suspension issue and complete window visual
  review. This is independent of the already visible Electron app.
- [ ] Connect observer rays and N1 trajectories to scientific image buffers and a
  capture/escape view; add sky sampling and HDR presentation afterward.
- [ ] Validate Schwarzschild shadow scale, Kerr silhouettes, escape direction, critical
  regions and budget sensitivity. Save classifications/error maps as well as color.
- [ ] Use persistent resources and asynchronous diagnostics in the production path;
  the synchronous headless test dispatcher is not the final frame scheduler.

Acceptance: actual native black-hole image, documented numerical domain/tolerances,
CPU/GPU agreement and reliable lifecycle; zero relevant validation errors. Preserve the
working desktop release until native output has independently passed these gates.

### N3 — Reference disk emission and spectra

- [ ] Add robust disk-crossing events, emitter motion, a documented thin-disk baseline,
  invariant frequency/intensity transfer and numerical spectral reference.
- [ ] Validate Novikov–Thorne/other selected model assumptions and boundary behavior,
  Planck integration/LUTs, multiple images and monochromatic versus bolometric transfer.
- [ ] Separate model parameters from decorative turbulence, bloom, exposure and grading.

Acceptance: formula specification, regression fixtures, converged spectral comparison,
bounded near-edge behavior and a reproducible reference image with approximations listed.

### N4 — Integrate native images into the product

- [ ] Prototype a measured presentation boundary: keep Electron as controls with a
  native rendering surface/process, or move rendering/UI to an appropriate native
  window stack. Do not assume zero-copy Vulkan embedding is automatic.
- [ ] Record latency, copy/readback cost, synchronization, platform/lifecycle behavior
  and ownership before selecting an architecture. Keep physics UI-independent.
- [ ] Reuse the D1 schema and D3 contract; preserve controls, experiments, captures and
  saved scenes. Compare results through N1–N3 fixtures, not aesthetic similarity alone.
- [ ] Offer explicit engine/model identification while multiple paths coexist. Retire
  an old path only after feature and validation gates pass, with a recoverable migration.

Acceptance: the user still launches a desktop app; feature parity is demonstrated;
native scientific output has its own evidence; performance improvement is measured.

## 7. Contracts to settle before combining engines

| Boundary | Required written decision/test |
|---|---|
| Units and spin | Native geometrized `M`, dimensional `a`, dimensionless `chi=a/M`, physical mass display, length/time conversion and valid bounds. Imported `massMsun` currently rescales labels while rendering uses `M=1`. |
| Component order | C++ uses `(t,x,y,z)`; imported GPU tetrads pack `(x,y,z,t)`. Explicit adapters and round-trip fixtures, never raw memory reinterpretation. |
| Momenta and rays | Covariant versus contravariant values, local frequency normalization, future/past direction, affine-parameter sign and camera forward basis. |
| Frames | Static/cinematic pose versus a physical observer worldline and transported tetrad; reject frames that do not exist in the selected region. |
| Time | Wall time, presentation animation time, affine parameter, proper time and coordinate time stay distinct; fixed export stepping is specified. |
| Ownership/reset | Immutable physical instance, generation-tagged IDs/jobs, cancellation and invalidation of trails/history/accumulation on changes. |
| Visibility | Coordinate overlays versus actual lensed, time-delayed images of moving objects. No implicit equivalence. |
| Approximation/model | Single Kerr, weak-field multi-mass, approximate binary, reference disk and artistic overlays carry explicit model/version labels. |
| Serialization/IPC | Bounded, validated schema; finite values; backward compatibility; fixed allowed operations; no arbitrary shell commands or renderer Node access. |

## 8. R1 — Packaging, regression and release

- [ ] Publish a release checklist for the chosen Explorer/Science scope with each
  requirement linked to evidence. Do not mark every phase done because a build passes.
- [ ] Verify a clean install/portable launch on another Windows account or machine,
  without development PATH, server, downloaded tools or source directory assumptions.
- [ ] Include version, app identity/icon, notices, source/asset provenance, useful local
  logs and a short guide. Confirm missing GPU features degrade visibly and safely.
- [ ] Review upstream rights before any public distribution; the original import lacks
  a license grant. Continue already-authorized local development without relitigating it.
- [ ] Add CI for TypeScript/build and meaningful native checks where supported; Linux
  remains a separate validation target. Do not claim coverage from an unexecuted preset.
- [ ] Decide installer/signing/updating only when preparing distribution. Keep a known
  good portable build and test upgrade/scene compatibility before replacing it.

## 9. Verification rules and known risks

| Change | Minimum relevant evidence |
|---|---|
| Desktop packaging/UI | TypeScript/build; real packaged window; affected controls; file loading; clear runtime errors; bounded resize/relaunch checks when relevant. |
| Scene files/bridge | Schema round-trips; invalid/stale/oversized input; cancellation/reset races; worker lifecycle; packaged runtime dependencies. |
| Camera/observers | Basis/tetrad/nullness conventions; known poses; time/input behavior; applicable native fixtures for physical observers. |
| GPU/physics | Existing affected suites plus independent reference/convergence and failure cases; documented tolerance/work budget; relevant driver validation. |
| Capture | Actual saved file decoded; dimensions/timing/metadata; cancellation; repeatability and resources. Clicking a button is not enough. |
| Performance | Controlled configuration, warmup, sample duration, frame-time distribution and quality/error comparison. |
| Documentation only | Check file links, current command semantics, dependency/status consistency; no need to rebuild an unchanged renderer. |

Persistent risks: source duplication across C++/TS/GLSL/WGSL; false confidence from
upstream screenshots/tests; invisible failed rays; ray divergence near critical orbits;
high-DPR render-target costs; session/time drift; Electron/native process boundaries;
stale output files; optional HQ capability differences; imported license status; native
AMD restore failures. Track actual reproduced issues with evidence and a next action.

## 10. Exact next session and handoff protocol

**Next task: D0.3–D0.6, finish desktop acceptance before a large new feature.**

1. Read this document, `AGENTS.md`, the latest desktop report and current files. Inspect
   changes since the last session; do not overwrite user work or edit generated assets.
2. Check current build/package state. Close only the app instance involved in rebuilding;
   do not terminate unrelated processes. Rebuild only if needed for the intended test.
   From a normal desktop terminal, run `node tools/check-desktop.cjs --measure
   --power-state "actual power source/profile"`. Record the new output directory;
   the 2026-10-14 restricted-session launch did not reach the renderer.
3. Verify PNG preview → native save → decode, and cancel/close. Fix observed failures
   and desktop wording. The previous Escape interruption is an uncompleted test, not
   a permanent permission denial; respect any new interruption immediately.
4. Verify scene reset and window lifecycle, including merger/free-fall exits and H/F11.
   Record exactly which checks ran; mark untested cases separately.
5. Update the evidence report/backlog, then finish D0.7–D0.9 (repeatable smoke checks,
   clean packaging and measured baseline). Keep each unfinished checkbox open. After
   D0 is accepted, start D1 with a small validated scene-state schema and save/load
   slice. D2 is the next visible exploration improvement.

At the end of each implementation session, update a compact handoff entry:

```text
Date / task IDs:
User-visible outcome:
Source files changed:
Generated package/version:
Checks run and results (with paths):
Known failures / unverified behavior:
Decisions and approximations:
Exact next action:
```

Mark checkboxes only when their stated scope is complete. Attach durable evidence to
acceptance claims. Preserve the user-facing desktop experience throughout the roadmap.
