# KerrLab Engineering Guide

This file is the persistent operating guide for humans and coding agents working on KerrLab.

## Mission

Build a research-oriented, interactive Kerr black-hole simulator in modern C++ and Vulkan. Correct relativistic physics, explicit conventions, numerical stability, validation, and honest diagnostics take priority over frame rate or spectacle. The renderer must derive its main image formation from integrated geodesics rather than a screen-space black-hole effect.

## Current product and continuation entry point (2026-10-12)

Read `ROADMAP.md` first when resuming future work. The user wants a standalone
Windows window and has authorized broad local reuse of `kyleyhw/black_hole`.
The current usable app is the imported WebGL renderer in an Electron desktop
shell under `web-demo/`; `start-black-hole.ps1` launches the desktop app by default.
The C++/Vulkan reference/native-rendering track remains separate and incomplete.
Keep the desktop app usable while developing it; do not restart a browser-only
demo or require a complete Vulkan rewrite before adding desktop features.

`ROADMAP.md` owns the current product backlog and exact next task.
`IMPLEMENTATION_PLAN.md` preserves native milestone history and implementation
details. `web-demo/PROJECT_PLAN.md` and imported validation reports are upstream
artifacts, not local task instructions or newly executed test evidence.
Read `benchmarks/results/2026-10-12-desktop.md` for actual evidence and open checks.
At handoff, update task status, verification, limitations and the exact next step.

Current host (2026-10-14): the active Arch folder is `KerrLab-Restored`.
Latest user-requested slice (2026-10-21): merger movie export at 60/144 FPS.
Read `MOVIE-EXPORT.md` and `benchmarks/results/2026-10-21-movie-export.md`.
The desktop now exposes a restricted sandbox preload for PNG export and external
FFmpeg encoding. Preserve sender/frame checks, ordered writes, fixed export
timing, cancellation/recovery and scene restoration. No encoder is bundled.
43 local tests pass; actual GPU movie acceptance is pending outside this session.
Read `RUN-ME.md` and `benchmarks/results/2026-10-14-desktop-diagnostics.md` for
the latest local changes, 28 passing regression tests, rebuild and remaining
desktop acceptance. `tools/check-desktop.cjs` provides a local packaged-app
check runner; its graphical path still needs execution outside the restricted
session that could not access GPU devices/start Electron's sandbox.

## Required workflow

1. Read `ARCHITECTURE.md`, `PHYSICS.md`, `NUMERICS.md`, `VALIDATION.md`, and the current milestone in `IMPLEMENTATION_PLAN.md` before changing physics or numerical code.
2. Check `THIRD_PARTY.md` before researching an upstream project again.
3. Inspect the current tree and preserve unrelated work.
4. Make the smallest coherent change that advances the current milestone.
5. Build with warnings enabled, run unit tests, then run applicable validation and benchmarks.
6. Fix correctness failures before performance regressions.
7. Update the persistent documents when a convention, architecture boundary, dependency, approximation, tolerance, benchmark, or provenance decision changes.

## Non-negotiable conventions

- Internal geometrized units use `G = c = 1`. Unless an API says otherwise, coordinates are measured in the configured geometrized mass unit `M`.
- Metric signature is `(-,+,+,+)`.
- Coordinate order is `(t,x,y,z)` for Cartesian Kerr-Schild and `(t,r,theta,phi)` for Boyer-Lindquist diagnostic values.
- Covariant momenta are stored as `p_mu`; the Hamiltonian is `H = 0.5 * g^{mu nu} p_mu p_nu`.
- Primary integration coordinates are ingoing Cartesian Kerr-Schild. Boyer-Lindquist coordinates must not be used as the only near-horizon evolution chart.
- The CPU reference path uses `double` or higher precision and conservative floating-point semantics. Never enable fast-math for reference or validation targets.
- GPU and CPU implementations must share a written formula specification and cross-validation vectors. Similar-looking formulas are not enough.
- Invalid states are classified and terminated or recovered; NaN/Inf must not propagate into later frame stages.
- Mass and spin are immutable properties of a simulation instance. Changing either creates a new instance and invalidates existing worldlines and accumulated images.
- Every artistic or phenomenological effect is labeled separately from physically derived output.

## Module boundaries

- `math`: dependency-free value types and numerical utilities.
- `physics`: spacetime, coordinates, observers, disks, and unit conversion. No Vulkan, windowing, UI, or renderer includes.
- `numerics`: integration, root finding, failure classification, and convergence diagnostics. Depends on `math` and abstract derivative systems, not Vulkan.
- `gpu_physics`: packed GPU representations and CPU/GPU formula conformance data.
- `renderer`: Vulkan resources, compute/raster passes, temporal reconstruction, and presentation.
- `scene`: environments, disk presentation, trajectories, and render-facing scene state.
- `instrumentation`: measurements, histories, benchmark data, and exported reports.
- `app`: platform-neutral orchestration.
- `platform`: isolated Windows/Linux window and OS integration.
- `ui`: controls and scientific panels; it consumes snapshots and commands rather than mutating physics internals directly.

Dependencies flow downward as described in `ARCHITECTURE.md`. Physics code must never depend on rendering or UI.

## Build and validation commands

Standard commands once a toolchain is available:

```text
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug --output-on-failure
```

The local `build.ps1 -Configuration Debug|Release|Sanitize` helper configures, builds, and runs CTest.
The CPU tools are `kerr_trace`, `kerr_worldline`, `kerr_session` and `kerr_benchmark`.
Nine suites cover geometry, geodesics, observers, adaptive frame transport, stationary
observers, bound orbits and sessions (95,463 checks). `build.ps1 -Vulkan -RequireValidation`
also builds `kerr_gpu` and runs shader-ABI and metric/derivative conformance tests.
`tools/validate-vulkan.ps1 -Device N` requires validation and enables synchronization checks.
`build.ps1 -Window -RequireValidation` also builds the GLFW/Vulkan diagnostic;
`tools/validate-vulkan.ps1 -Window -Device N` checks geometry and window lifecycle.
Read `benchmarks/results/2026-10-07-window.md` for intermittent AMD suspension timeouts
and unavailable native screenshot review before claiming full window validation.
Read `GPU_PHYSICS.md` before changing packed layouts or shaders. The headless dispatcher
has an explicit 16,384-group application budget independent of driver limits.
The Linux debug preset exists but has not yet been executed in CI. Do not commit build output,
downloaded SDKs, or local toolchains. The resumable status and next work are in `IMPLEMENTATION_PLAN.md`.

## Third-party policy

- Inspect the actual license file at a pinned revision before copying code.
- A README license claim without a license grant is insufficient.
- Record source, revision, license, files used, modifications, and notices in `THIRD_PARTY.md` before merging reused code.
- GPL/AGPL code is reference-only unless the whole-project licensing decision explicitly permits incorporation.
- No candidate implementation code has been copied into the initial physics milestone.

## Definition of done for a change

A change is done only when its applicable tests pass, numerical behavior is bounded, failure behavior is explicit, documentation matches the implementation, and performance claims have measurements. Compilation alone is not completion.
