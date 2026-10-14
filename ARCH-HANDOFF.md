# KerrLab(Arch): next agent handoff

Latest continuation (2026-10-21): user explicitly prioritized movie export,
including 12-second 60 FPS and 144 FPS clips. The rebuilt desktop has **Render
movie**, fixed full-resolution frame capture, PNG recovery and external FFmpeg
MP4 encoding. See `MOVIE-EXPORT.md` and
`benchmarks/results/2026-10-21-movie-export.md`. All 43 local tests pass; actual
graphical movie acceptance is pending because this session cannot start Electron's
sandbox/access DRM devices. Next: `node tools/check-desktop.cjs --movie` from a
normal desktop terminal, followed by a full-resolution visual/export check.
The prior handoff below is historical and its task ordering is superseded by this
explicit user request for the movie slice.

Date: 2026-10-13. Read this, AGENTS.md, then ROADMAP.md. This is a self-contained
project continuation record; do not require the old chat, Windows account path,
or old Codex memory folder. Resolve the project root from this file's location.

## User decisions and standing project scope

The user is formatting the laptop and installing Arch Linux. They requested a
complete portable migration named **KerrLab(Arch)** and broad autonomous future
local development. Continue from the existing code and roadmap. They want a
standalone desktop window, substantial creative additions, and extensive local
reuse of the already imported `kyleyhw/black_hole` source. Another project version
was submitted; the immediate old deadline no longer drives this branch.

Ordinary local edits, refactors, dependency setup, builds, debugging and testing
within this project are authorized. Do not ask again for these routine actions.
Make decisions, complete the necessary work and record verification. Ask only
when genuinely missing information or a new consequential action requires it.
These instructions do not override the active tool/client policy. Do not modify
global permission settings, passwordless sudo, or the OS to manufacture access.
`continue-kerrlab-arch.sh` starts the user-requested CLI full-access session without
changing global configuration. Do not interpret scope as permission to publish,
contact people, purchase services or wipe other files.

Preserve scientific/observatory presentation. Keep validated reference physics
separate from imported rendering approximations and artistic effects. KerrLab is
independent of the separately discussed KerrBlackHole OpenGL repository.

## Current implementation

- Electron 44.7.0 loads bundled local files, with sandbox/context isolation on;
  no Node integration, web server or remote content in the renderer.
- Linux app name/title is KerrLab(Arch), binary `out/desktop/KerrLab-Arch/kerrlab-arch`.
  Windows output remains `out/desktop/KerrLab/KerrLab.exe` in the original Windows
  workspace; Windows generated binaries are not in this Arch backup.
- GLSL WebGL2 single Kerr and approximate binary scenes, event merger animation,
  orbit/free-fall cameras, disk/stars/bloom, four presets, diagnostic controls,
  PNG preview/native save handling, and an optional separate HQ WebGPU path exist.
- Native C++ core has null/timelike tracing, observers, transport, clocks/sessions,
  CLI CSV output and conformance tests. Native Vulkan currently provides metric
  conformance and a diagnostic window, not a native black-hole renderer.
- Linux launch, setup, app-menu, diagnostics, offline restore and CPU build helpers
  are implemented. `tools/package-desktop.cjs` validates the target binary and
  stages replacement in separate Windows/Linux output directories.
- `tools/package-arch-migration.py` produces an allowlisted source/runtime backup,
  internal manifest, Linux executable modes and a verified tar archive. Repacking
  uses new output paths; it refuses to overwrite an existing backup.

## Most urgent unresolved issue: actual merger speed

The user reported 1–2 fps. A Windows accessibility-tree observation confirmed 2 fps
with GW151226, inspiral separation 10.02 M, fGW 87.3 Hz and t-tmerger -85.3 ms.
The Play button indicated the animation was paused at that observation. Selected
GPU, physical viewport and exact quality controls were not recorded; do not call
this a controlled benchmark. Hardware exposed Radeon 740M + RTX 3050 6 GB Laptop.

The new `web-demo/src/merger-quality.ts` controller starts binary scenes at a
bounded pixel count and adjusts internal resolution from visible RAF intervals:
192k initial / 300k maximum frozen binary, 96k initial / 192k maximum retarded,
48k floor, capped by manual requested resolution. It targets 30 fps with rate limits
and hysteresis. UI stays full resolution. It never changes max ray steps, formulas
or simulation speed. Manual quality and test mode can opt out.

The >2-second foreground interval recovery bug was fixed: one isolated gap is
ignored, consecutive slow foreground intervals trigger downshifts. **15/15**
controller tests pass. Physics shaders remain unchanged. The HUD has a known
pre-existing capture/bench timing bias; avoid taking FPS evidence immediately
after synchronous capture. Actual post-fix FPS has not been verified.

The last Windows UI attempt was stopped by the user's physical Escape key.
Do not claim that final rendering/saving/performance checks passed. A new session
can perform fresh target verification within the user's active continuation.

## Exact next work on Arch

1. Verify the transferred checksum/manifest before edits. Run doctor, establish
   desktop/display/GPU information, launch the packaged app and inspect the actual
   image. Test default display and the relevant X11/Wayland option.
2. Confirm the active GPU; compare default versus configured prime-run if present.
   Reproduce GW151226, record output/internal dimensions, settings, power mode and
   frame times after warm-up. Verify automatic detail on/off, resize, play/pause,
   retarded mode, scene reset and background/resume. Do not promise 30 fps.
3. Fix remaining compatibility/performance problems with measured evidence. No
   silent software renderer fallback, no automatic reduction in physics accuracy,
   no shader formula changes without the physics validation workflow.
4. Exercise PNG preview → native save, cancel, close, Escape/focus, fullscreen,
   first startup and error handling. Run the full offline rebuild once on Arch.
5. Install the native toolchain and run Release/Debug/Sanitize CPU suites. Record
   current Linux counts/results; old Windows results are historical only.
6. Update ROADMAP D0/D8 acceptance evidence, then proceed through its unfinished
   desktop features (sessions, free flight, observer bridge, particles/clocks,
   capture paths/video, ray inspector) and the separate native milestones.

## Commands and evidence

See ARCH-START-HERE.md for runnable commands. Use `bash start-kerrlab-arch.sh --build
--no-open` for an offline desktop rebuild and `bash tools/build-native.sh` for
native Release build+CTest. Add the relevant option only for Vulkan/native-window
work. Git was installed on Windows, but the original source root had no `.git`;
do not claim there are commits or a remote. Initialize version control in the
restored workspace if useful, preserving the migration archive as the baseline.

Read `benchmarks/results/2026-10-13-arch-migration.md` and
`benchmarks/results/2026-10-13-merger-performance.md`. Existing upstream `bench/`,
`e2e/` and `validation/` reports were imported, not rerun. Native historical count
95,463 checks / nine suites is not Linux evidence. The previous AMD native
restore/suspension timeout remains open, independently of Electron success.

Upstream pinned revision is `96a6835f2624bc6425682f723a050f397893cb6d` with original
hash manifest and immutable archive under `.tools/upstream-black-hole`. Preserve
attribution and existing license findings in THIRD_PARTY.md/UPSTREAM.md; do not
repeat the already resolved local-reuse permission question or invent a license.

At each handoff, update measured results, limitations and the next concrete step.
