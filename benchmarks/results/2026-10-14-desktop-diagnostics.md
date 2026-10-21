# Desktop diagnostics continuation — 2026-10-14

Task IDs: D0.4, D0.7 and a partial D8.1 measurement foundation.
Workspace: `/home/abrsdxd/Projects/KerrLab-Restored`.

## Delivered

- Corrected the Electron 44 `console-message` callback: its first argument holds
  structured severity/message/source data. The previous callback inspected the
  deprecated second argument and missed warnings/errors. Only serializable fields
  are written, excluding the live frame object.
- Missing packaged HTML fails before a blank window opens. Main-page load failures
  show repair guidance; an initial load rejection does not show a duplicate dialog.
  Renderer startup/runtime failures are also logged, including unhandled promises.
- FPS now comes exclusively from visible RAF intervals. Synchronous capture and
  benchmark draws never count as displayed frames. Capture completion, hidden
  windows and workload/viewport changes clear timing history; queued timestamps
  older than the capture completion are excluded. FPS uses frames/elapsed time,
  with uncapped foreground stalls and a bounded history. The HUD uses the latest
  120 intervals and shows p95 milliseconds in its tooltip.
- Added `window.__bh.diagnostics()` and `resetFrameTimings()` to the existing local
  test seam. Snapshots contain raw intervals, mean/p50/p95/p99/max, actual dimensions,
  quality, camera, scene/merger state and WebGL graphics identity.
- Added `tools/check-desktop.cjs`, which launches the pinned runtime against the
  packaged shell/assets with an isolated profile. It can collect scene/PNG evidence
  and controlled single-Kerr/GW151226 timing reports without a browser server or
  external automation dependency. See `web-demo/e2e/DESKTOP-CHECKS.md` for exact
  coverage, measurement conventions and remaining manual acceptance.

The prior 0.6 default resolution is preserved. Physics shaders, integration budgets
and native reference code were not modified. RAF measurements include compositor
scheduling and presentation waits; they are not GPU execution times. No performance
improvement or target FPS is claimed from this session.

## Executed verification

| Check | Result / durable evidence |
| --- | --- |
| Timing and desktop event regression tests plus existing quality tests | **28/28 passed**: 7 timing, 6 desktop-shell, 15 quality. `out/verification/2026-10-14-diagnostics/tests.log` |
| Offline restoration, TypeScript, Vite production build, tests and Linux packaging | **Passed** through `bash start-kerrlab-arch.sh --build --no-open`. `out/verification/2026-10-14-diagnostics/build.log` |
| Desktop harness syntax and help | `node --check tools/check-desktop.cjs` and `--help` passed |
| Runtime doctor | Basic prerequisites passed; pinned binary and shared libraries present. This does not establish access to GPU devices in the active sandbox. |
| Fresh desktop/harness launch | **Unavailable in this session**: native process terminated with SIGTRAP before its JavaScript entry, after reporting missing DRM devices and sandbox `shutdown: Operation not permitted`. `out/desktop-check-OPWU01/runtime.log` and `launcher.json` |

Detailed test command:

```bash
node --test --test-isolation=none web-demo/tests/*.test.ts web-demo/tests/*.test.cjs
```

The ordinary process-isolated runner reports three passing test files here; the
command above records all 28 individual checks. The shell tests use isolated
Electron event/I/O fixtures. They do not prove real native dialogs or window behavior.

Generated package: `out/desktop/KerrLab-Arch/kerrlab-arch`, Electron 44.7.0,
packaged at **2026-10-14T10:38:34.501Z**. `BUILD-INFO.json` correctly retains
`runtimeTestedOnTarget: false`. No new screenshot, PNG save/cancel acceptance or
GPU benchmark was produced; the earlier restore's visual check is historical.

## Handoff

Source changes: `web-demo/desktop/main.cjs`, `web-demo/src/main.ts`,
`web-demo/src/boot.ts`, new `web-demo/src/frame-timing.ts`, two new test files,
`web-demo/package.json`, `start-kerrlab-arch.sh`, and new `tools/check-desktop.cjs`.
Documentation/provenance and the authoritative roadmap were updated.

D0.3–D0.9 remain open where real desktop evidence is required. The new harness is
implemented and syntax checked, but its graphical path has not completed on the
target desktop. D8.1 remains partial: actual GPU timer queries, complete memory
tracking and a measured baseline are not delivered.

Next action from a normal laptop desktop terminal:

```bash
node tools/check-desktop.cjs --measure --power-state "AC, balanced"
```

Use the actual power source/profile. Inspect generated PNGs and the report, then
exercise the real Save dialog, physical H/Escape/F11, minimize/restore and relaunch.
Compare the intended X11/Wayland backend and selected GPU before tuning merger
resolution further. Preserve failed reports and fix observed failures. Once D0
has supporting evidence, proceed to D1's versioned scene schema and save/load slice.
