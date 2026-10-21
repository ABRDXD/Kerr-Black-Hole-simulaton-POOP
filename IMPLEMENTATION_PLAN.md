# Implementation Plan

Last updated: 2026-10-12.

## Current user priority: standalone desktop app and full future roadmap

**Read [ROADMAP.md](ROADMAP.md) for the authoritative future-work order.** It
contains the desktop feature phases D0–D9, native image phases N1–N4, release
gate R1, dependency/validation contracts and a concrete next-session checklist.
This file retains the detailed native implementation history below.

The user authorized full local import of `kyleyhw/black_hole` and then specified
a standalone window. The pinned renderer in `web-demo/` is now packaged in
Electron 44.7.0, loading files directly. The default launcher opens the desktop
app; optional browser preview requires `-Browser`. Basic native-window image
inspection succeeded for this Electron app. PNG save, broader controls/window
checks and optional WebGPU acceptance remain open. See
`benchmarks/results/2026-10-12-desktop.md`, `THIRD_PARTY.md` and `web-demo/UPSTREAM.md`.

The usable desktop-rendering milestone is separate from native C++/Vulkan
image formation. Native tests do not validate the imported renderer. Next:
ROADMAP D0.3–D0.6 (desktop acceptance), then D1 scene state/persistence and D2
free-flight exploration. Preserve the native status recorded below.

## Current state

- [x] Workspace inspected; no pre-existing project or Git repository found.
- [x] Initial candidate repositories and license status audited.
- [x] Persistent architecture, physics, numerical, rendering, performance, provenance, and validation documents created.
- [x] CMake/C++20 foundation designed without external runtime dependencies.
- [x] Build tools bootstrapped on the current Windows machine.
- [x] Milestone 2 physics tests compiled and passing (271 checks).
- [x] CPU automatic metric derivatives and Hamiltonian null geodesics.
- [x] DOPRI5 adaptive solver with quartic dense output, multiple-root event isolation, and typed failures.
- [x] Independent eighth-order modified-midpoint extrapolation reference and RK4 benchmark baseline.
- [x] Analytic, finite-difference, invariant, convergence, event, and failure tests (6,645 checks).
- [x] Debug, Release, and address/undefined-behavior sanitizer builds pass on Windows.
- [x] CLI tracer, six exported example trajectories, and reproducible CPU benchmark report.
- [x] Physical observer tetrads, camera rays, and Doppler/aberration checks.
- [x] Adaptive timelike worldlines with massive Carter invariant and both clock readings.
- [x] Coupled geodesic parallel-transport primitive with analytic gyroscope validation.
- [x] Worldline CLI with orbit/infall/pair CSV export and finite monotone clock checks.
- [x] All five suites pass in Debug, Release, and ASan/UBSan (50,645 checks per configuration).
- [x] Existing CPU benchmark rerun; work and numerical columns unchanged at reported precision.
- [x] Adaptive coupled worldline/orientation tracing with dense physical and coordinate-time events.
- [x] Fixed-coordinate accelerated observers with Fermi-Walker axes and analytic proper clocks.
- [x] Bounded observer sessions, shared coordinate slices, lifetime invariant gates, and reset generations.
- [x] Session CLI exporting independent clocks, diagnostics, and transported tetrads.
- [x] Independent equatorial eccentric Kerr orbit quadrature reference and 20-cycle stress validation.
- [x] Nearby coordinate-target precision bug reproduced and fixed without snapping clocks.
- [x] Pinned Vulkan headers, standalone glslang and local validation/SPIR-V tools with retained notices.
- [x] Headless Vulkan 1.3 device probe, dynamic loader, bounded compute dispatch, mapped readback and timestamps.
- [x] Versioned binary32 metric/analytic-derivative shader, C++ layout assertions and SPIR-V ABI checks.
- [x] 723-case / 58,302-value conformance passes on Radeon 740M and RTX 3050 with validation enabled.
- [x] Partial/reordered/repeated batches, 27 ABI rejection cases, failed-output clearing and host budget gates.
- [x] AMD-advertised huge dispatch limit exposed and fixed with an independent application work ceiling.
- [x] Debug, Release and ASan/UBSan pass 95,463 CPU checks plus two GPU/ABI suites; CPU benchmark numerical/work columns unchanged.
- [x] Pinned GLFW window/surface adapter and persistent Vulkan presentation diagnostic.
- [x] NVIDIA resize/minimize/restore and exact three-extent image oracle pass in Debug, Release and ASan/UBSan (12 suites).
- [x] Both GPUs complete the strict release window lifecycle and exact image oracle.
- [ ] Earlier AMD suspension timeouts explained and native visual review completed.

The first build exposed non-bitwise metric symmetry from repeated products; the metric now mirrors
its upper triangle. Local runtime DLLs are copied next to executables so CTest and direct launching
work without modifying system PATH. Tools are LLVM-MinGW 20260922 (Clang 23.1.2), CMake 4.4.3, Ninja 1.13.2.

### Native track resume point: Milestone 5 presentation, compatibility gate open

Implemented CPU scope: physical observer events, Eulerian/boosted/stationary/circular tetrads,
local future/past camera rays, massive Carter and constraint diagnostics, adaptive timelike
worldlines with parallel-transported axes, and stationary Fermi-Walker transport. `ObserverSession`
owns independent bounded histories on common coordinate-time slices, freezes individual failures,
preserves birth invariants across advances, and invalidates IDs on spacetime reset.
`kerr_worldline` exports trajectories; `kerr_session` additionally exports transported tetrads.

The new `kerr_gpu` tool probes devices and runs the metric/derivative conformance
shader. `GPU_PHYSICS.md` is the v1 formula and buffer contract; reflection checks
the compiled shader before dispatch. `ComputeContext` is synchronous validation
infrastructure with no physics dependency. Both local GPUs pass strict Khronos
and synchronization validation. Results and remaining limits are in
`benchmarks/results/2026-10-07.md`.

The GLFW 3.4 window adapter, shared Vulkan context and persistent compute-to-
swapchain diagnostic are now implemented. NVIDIA passes 24-frame lifecycle tests
with two resizes, minimize/restore and three exact GPU-image readbacks. AMD also
completed the final strict release run, but earlier runs repeatedly entered the
suspended state after restore and timed out, with zero acquisition retries.
The intermittent cause is unresolved. Native desktop capture was unavailable.
The window gate is therefore **partial**;
see `benchmarks/results/2026-10-07-window.md` for reproduction and limits.

Next coherent work within the native track (product priority is in ROADMAP.md):

1. Diagnose intermittent AMD suspension after restore on an active
   desktop. Obtain native screenshots and manual resize/close review. Preserve
   KHR presentation-fence lifetime rules; do not mask the failure by skipping
   frames, disabling validation or loosening the deadline without evidence.
2. Extend versioned GPU conformance to Hamiltonian RHS and single integration
   steps before adding complete GPU null trajectories. Keep pointwise geometry
   tolerances separate from trajectory/event error budgets.
3. Connect validated GPU null trajectories to a capture/escape image. The window
   already has persistent resources, but the headless conformance runner remains
   synchronous and allocates per dispatch.
4. Preserve all CPU validation/baselines. Extend external fixtures to inclined
   eccentric bound orbits and run Linux validation; the current separated CPU
   orbit reference covers equatorial eccentric orbits only.

CPU geodesics, observer sessions and headless GPU geometry are usable. Native
Vulkan geodesic integration and black-hole image formation, native window
compatibility/visual review, integrated observer UI, reference radiative disk
emission, performance governor and Linux validation remain. The separate
Electron/WebGL desktop image is already working.

The toolchain uses Vulkan-Headers 1.4.321, GLFW 3.4, glslang 15.0.0, and Khronos
validation/SPIRV-Tools from Windows SDK 1.4.321.1. API 1.3 plus explicitly checked
presentation extensions remains the runtime contract. The driver supplies the
Vulkan loader. `tools/bootstrap-vulkan.ps1 -Window -Validation` reproduces the
dependencies; `build.ps1 -Window -RequireValidation` runs strict CTest, and
`tools/validate-vulkan.ps1 -Window -Device N` checks a selected GPU. Omit `-Window`
from the helper for geometry-only checks. CPU-only builds remain independent.
Git is installed as of the 2026-10-12 review, but this project has no `.git`
directory. Local tools and build output stay ignored under `.tools/` and `out/`.

## Highest-risk components

1. Consistent, stable Cartesian Kerr-Schild metric derivatives on CPU and GPU near the horizon, axis, ring, and extremal spin.
2. Adaptive GPU integration without unacceptable divergence, missed thin-disk/horizon events, or unreported critical-ray error.
3. Correct tetrads for arbitrary moving observers, including degeneracy handling and orientation transport.
4. Mixed precision on an RTX 3050: photon-region rays are sensitive while native FP64 throughput is weak.
5. Separating radiative-transfer physics from visual disk embellishment and reconstructing spectra without aliasing.
6. GPU/CPU agreement with independent reference vectors instead of self-consistency alone.
7. Stable temporal reconstruction for strongly lensed features and moving observers.

## Milestone 1 — project and evidence foundation

Deliverables: documentation, source layout, warning-clean CMake targets, deterministic test harness, dependency policy, platform/toolchain inventory. Vulkan initialization is deliberately postponed until the SDK is available rather than replaced with random renderer scaffolding.

Gate: configure/build/test on Windows; CI design documented for Linux.

## Milestone 2 — Kerr geometry and units (complete)

Deliverables: validated parameters, stable Cartesian Kerr-Schild radius, covariant/inverse metric, horizon and ergosphere, ISCO and equatorial photon landmarks, physical scaling.

Gate: all checks in the foundation section of `VALIDATION.md`, sanitizers where supported, and no fast-math.

## Milestone 3 — CPU Hamiltonian null geodesics (CPU gate complete)

1. Implemented forward-mode automatic derivatives, checked against analytical Schwarzschild derivatives and five-point finite differences.
2. Implemented DOPRI5 and an independent eighth-order extrapolated-midpoint reference.
3. Implemented scaled error control, quartic dense output, polynomial root isolation, bounded rejection, and typed failure records.
4. Added radial analytical rays, circular Kerr photon orbits, near-critical capture/escape, disk events, invariants, convergence, and failure regressions.
5. Benchmarked DOPRI5, eighth-order reference, and fixed RK4. DOPRI5 is the current CPU tracing method; GPU production selection and an optional alternate recovery method remain measurement-dependent.

Gate: null constraint/invariant tolerances met across weak, strong, capture, escape, disk-crossing, and near-critical sets; failure injection never leaks NaN.

## Milestone 4 — observers and timelike worldlines (CPU core implemented; in progress)

Deliverables: tetrads, stationary/orbital/free-fall/geodesic observers, proper and coordinate clocks, orientation transport, timelike particles, trajectory inspector data, multiple independent observers.

Gate: tetrad and worldline validations plus special-relativistic far-field limits.

## Milestone 5 — Vulkan platform and GPU physics

Deliverables: Vulkan 1.3 instance/device/queues, validation layers in debug, swapchain, compute pipeline, packed-state reflection checks, timestamp queries, GPU metric/derivative/integrator kernels, CPU/GPU conformance runner.

Gate: headless conformance dispatch where supported; visible Windows/Linux smoke test; zero validation errors; quantitative error report.

## Milestone 6 — sky, shadow, and lensing

Deliverables: observer-ray construction, capture/escape map, environment sampling, procedural deterministic/non-deterministic star modes, native HDR presentation, shadow and photon-region diagnostic views.

Gate: Schwarzschild shadow size, Kerr reference silhouettes, convergence maps, and no hidden invalid-ray coloration.

## Milestone 7 — relativistic accretion disk

Deliverables: Novikov-Thorne baseline, physical/manual modes, robust disk events, emitter motion, invariant redshift and intensity transfer, spectral reference and accelerated LUT path.

Gate: disk physics regression suite, spectral comparison, documented approximations, multiple-image tests.

## Milestone 8 — interaction, observers, and particles

Deliverables: session reset semantics, camera controls, observer switching, probes/test particles, pause/step/time scaling, configuration save/load, trajectory visualization.

Gate: reproducible configs and independent proper-time histories.

## Milestone 9 — scientific interface

Deliverables: cinematic/scientific/hybrid modes; toggleable panels, plots, overlays, ray classifications, redshift/error maps, clock comparison, actionable failure reports.

Gate: UI does not create synchronous per-frame GPU readbacks and never mislabels quantities.

## Milestone 10 — performance governor

Deliverables: GPU timing/workload histories, separate quality axes, target FPS, hysteretic documented decisions, workload/resolution/sample controls.

Gate: benchmarked 30/60/120 targets where achievable; reference accuracy is invariant under governor activity.

## Milestones 11–12 — advanced rendering and release

Add volumetric/finite-thickness disk options, advanced spectra, galaxies, improved caustic/star filtering, optional DLSS after license and artifact review, packaging, Linux CI, broad validation, profiling cleanup, and learner-oriented guides.

Final release requires all user-requested core systems; roadmap checkboxes are not substitutes for implementation.
