# Validation Plan

## Desktop evidence boundary (2026-10-12)

The packaged Electron/WebGL app has passed production build/TypeScript checks
and prior visual inspection of a real black-hole image in its desktop window.
See `benchmarks/results/2026-10-12-desktop.md` for the retrospective record.
The observed 31–33 fps is a sampled UI counter, not a controlled benchmark.
PNG save/cancel, full desktop controls/lifecycle and optional WebGPU stills
remain open acceptance tasks in `ROADMAP.md` D0.

Imported `web-demo/e2e`, `bench` and `validation` reports are historical upstream
evidence, not local Windows runs. The imported Linux-specific harness has not
been adapted and executed here. Native CPU/Vulkan results below do not validate
the imported TS/GLSL/WGSL paths. Electron window inspection also does not resolve
the native GLFW/Vulkan AMD suspension issue.

## Current continuation gate (2026-10-07)

Nine CPU suites pass 95,463 checks in Debug, Release and ASan/UBSan: foundation
271, null geodesics 6,645, observers 12,449, timelike 31,243, transport 37,
adaptive transport 1,160, stationary 161, bound orbits 42,951, sessions 546.
The older milestone-specific counts below are historical subsets.

With Vulkan enabled, shader ABI and headless geometry add two suites. The ABI
gate rejects 27 malformed/incompatible variants. Both Radeon 740M and RTX 3050
pass 723 fixtures / 58,302 floating comparisons, exact symmetry, finite/zero
payload rules, 1/63/64/65-record/reversed batches and host work-limit rejection.
Strict geometry runs enable Khronos and synchronization validation with zero
reported validation errors; SPIRV-Tools validates the compiled module.

The [report](benchmarks/results/2026-10-07.md) contains hardware, quantitative
errors, the corrected AMD work-budget failure and remaining limitations. GPU
geometry formulas/acceptance bounds are in GPU_PHYSICS.md. GPU integration,
physical images, past-ray capture, Linux and noncoherent-memory hardware remain
outside this acceptance gate. Sanitizers cover project CPU code, not GPU execution
or uninstrumented vendor/validation-layer binaries.

The new window suite requires 24 presented frames, at least three swapchain
generations, observed minimization with no new submissions, restore, and exact
RGBA8 readback at 1001x613, 817x479 and 1023x617. All three extents exercise partial
16x16 workgroups. The oracle checks 1,636,147 pixels / 6,544,588 channel values.
Khronos 1.4.321 from Windows SDK 1.4.321.1 and synchronization validation are
enabled. Both GPUs complete the full lifecycle and all exact readbacks. Earlier
AMD runs timed out in the suspended state with zero acquisition retries, also
with validation disabled; the intermittent cause remains open.
Readback is from the compute image, not the swapchain or desktop. Native visual
inspection could not run because the Computer Use native pipe was unavailable.
See the [window report](benchmarks/results/2026-10-07-window.md). This partial
presentation gate does not validate GPU geodesics, lensing or disk radiance.

## Principles

Validation is layered so a good-looking image cannot hide a broken metric or integrator. Deterministic tests use fixed seeds and versioned configurations. Visual stochastic variation is disabled or isolated from the physics checks.

## Foundation checks (implemented)

- parameter validation (`M > 0`, finite values, `|a| <= M`);
- stable implicit Cartesian Kerr-Schild radius and defining-polynomial residual;
- oblate-coordinate identity away from degeneracies;
- Kerr-Schild null covector under Minkowski metric;
- covariant/contravariant metric product equals identity;
- metric symmetry, finiteness, determinant `-1`, asymptotic flatness;
- Schwarzschild and extremal limits for horizons, ergosphere, ISCO, and equatorial photon radii;
- solar-mass geometrized length/time conversion.

## Geodesic checks (implemented CPU null and timelike subsets)

| Case | Expected measurement |
|---|---|
| Minkowski (`M -> 0` test metric) | straight affine trajectory; exact constant momentum |
| radial Schwarzschild null ray | agreement with analytical/independent reference relation |
| Schwarzschild critical impact | shadow boundary tends to `b=3 sqrt(3) M` for distant observer |
| equatorial Kerr circular photon orbit | bounded radial residual at known prograde/retrograde radii |
| generic Kerr null ray | `H`, `E`, `L_z`, Carter quantity drift within mode tolerance |
| timelike circular orbit | expected angular velocity and normalization |
| bound timelike orbit | frequencies/turning points against independent reference vectors |
| convergence sweep | observed order matches the selected integrator away from event discontinuities |

## Observer checks (implemented CPU)

- tetrad orthonormality and handedness;
- `e_(0)=u` and future-directed normalization;
- locally constructed camera rays are null after coordinate transformation;
- aberration and Doppler limit agree with special relativity far from the hole;
- stationary-observer rejection inside the ergoregion;
- proper-time integration convergence for orbital and infalling observers.

## Disk and transfer checks

- ISCO coupling and explicit override labeling;
- circular emitter four-velocity normalization;
- redshift invariant computed identically in equivalent frames;
- `g -> 1` for comoving emitter/observer in the flat limit;
- Planck and spectral lookup values against direct high-precision integration;
- Liouville invariant transport and distinction between monochromatic `g^3` and bolometric integration;
- Novikov-Thorne flux limiting/regression values and zero-torque inner boundary.

## CPU/GPU agreement

The CPU generates versioned conformance vectors containing positions, momenta, metric/inverse, derivatives, Hamiltonian derivatives, one-step results, event locations, and final classifications. GPU tests report absolute, relative, and ULP-like errors where meaningful. Acceptance tolerances vary by quantity and precision mode and are recorded with the result.

Image comparisons include scientific buffers, not only RGB: termination class, escape direction, disk hit coordinates, redshift, step counts, and constraint residual.

## Regression artifacts

Small numeric datasets and hashes are committed. Large images/traces are generated by scripts and stored outside Git unless a compact, reviewed fixture is necessary. Every baseline records formula version, compiler semantics, configuration, seed, and expected tolerance; baselines are not updated merely to make a failing test green.

## Historical CPU acceptance gate (2026-09-25)

The foundation, null-geodesic, observer, timelike, and geodesic orientation-transport suites pass
on Windows as of 2026-09-25. This is not validation of a complete adaptive orientation driver,
accelerated-observer transport, disk emission model, renderer, GPU implementation, or full domain.

| Configuration | Foundation | Null geodesic | Observer | Timelike | Transport | Result |
|---|---:|---:|---:|---:|---:|---|
| Clang Debug | 271 | 6,645 | 12,449 | 31,243 | 37 | pass |
| Clang Release | 271 | 6,645 | 12,449 | 31,243 | 37 | pass |
| Clang Debug + ASan/UBSan, no recovery | 271 | 6,645 | 12,449 | 31,243 | 37 | pass, no sanitizer reports |

The numerical tests cover:

- AD metric derivatives versus analytical Schwarzschild values and five-point finite differences at
  exterior, horizon-near, interior, axis, equatorial, and negative-z points for spins `0, +/-0.83,
  0.999999, 1`. Finite-difference discrepancy threshold is `2e-8 max(1,|partial|)`.
- DOPRI5 fifth-order and extrapolated-midpoint eighth-order convergence on an analytical ODE;
  quartic dense output; flat straight rays; exact momentum preservation; non-finite stage rejection.
- Polynomial isolation of two same-endpoint-sign crossings and all eight roots of a test polynomial.
- Analytical ingoing Schwarzschild rays (`r=r0-lambda`, `t=lambda`), analytic outgoing coordinate
  time, and smooth continuation to `r=1.5M` when horizon termination is disabled.
- Capture versus escape at `b=3 sqrt(3)M +/- 1e-3M` and `+/-1e-6M`. This is not a claim of resolved
  classification arbitrarily close to the critical boundary.
- Prograde and retrograde circular photon orbits at spins `0, +/-0.5, 0.99`, checked against the
  independent angular-velocity relation over 12 affine units; radial/position bounds `2e-7/3e-7`.
- Generic inclined Kerr rays at spins `+/-0.5, 0.99`: DOPRI5 endpoints within `2e-8` absolute of
  eighth-order reference endpoints; reference step-halving differences within `2e-11`.
- Energy, axial angular momentum, null Carter invariant, the Schwarzschild axis limit, and a
  forward/reverse trajectory round trip.
- Disk-event refinement, null/non-finite input rejection, zero momentum rejection, work limits,
  bounded cancellation, invalid configuration, minimum-step failure, and rejection limits.

Six release CLI scenarios terminate as expected (`capture`, `escape`, both critical cases, `kerr`,
and `disk`) and export finite CSV trajectories. A negative-spin disk example also passes. Invalid
spin and NaN tolerance arguments return exit code 1. Numerical runtime failure returns exit code 2.

Default null residual/invariant gates are `1e-7`; the six sample trajectories measured maximum
normalized null residual below `9e-10`. Exact tolerances and normalization are in `NUMERICS.md`.
`benchmarks/results/2026-09-24-cpu-integrators.csv` records endpoint error, constraint error, work,
reference-pair sensitivity, and timing. Two converging reference runs are empirical evidence,
not a certified global error bound.

The new CPU suites additionally cover:

- Eulerian and boosted tetrads at signed and extremal spins, on/near both axis directions,
  horizon-near and interior events, and large radius; independent determinant and orthonormality
  contractions, future orientation, and rejection of invalid velocities/frames.
- Camera-ray nullness, local frequency, explicit future/past spatial-direction convention,
  special-relativistic Doppler and aberration in local frames and the far-field limit.
- Circular observer angular velocity, analytic energy/angular momentum and coordinate-clock
  rates across three masses and signed spin, with unavailable circular/stationary states rejected.
- Full timelike circular periods for `M=1,2.3`, spins `0,+/-0.8,0.999`, both orientations;
  radius, Cartesian return, and coordinate clocks within `4e-7 M`, Hamiltonian within `2e-9`.
- Schwarzschild `E=1` radial infall proper/coordinate clocks, horizon location and continuation
  to `r=1.5M`; radius within `3e-8 M`, coordinate clock within `5e-8 M`, and `u^t=1.5` at the horizon.
- Inclined timelike endpoints for spins `0,+/-0.8,0.999`: DOPRI5 within `2e-8` absolute of midpoint8,
  with independent-method step-halving sensitivity below `2e-11`; massive Carter axis term,
  separate elapsed clocks, cancellation, work/rejection/minimum-step limits, and the large-momentum
  unit-mass normalization trap.
- Coupled geodesic/frame transport through one Schwarzschild circular period at `r=10M`: radial
  and tangential projections agree with `cos(2pi sqrt(1-3M/r))` and `-sin(2pi sqrt(1-3M/r))` within
  `2e-9`; midpoint8 steps `0.5M` and `0.25M` differ by less than `2e-10` in all 20 components.
  Kerr forward/reverse transport differs by less than `3e-11`; radial transport crosses the horizon
  to `r=1.7M`. Corrupted frames and non-finite states fail without projection or NaN output.

The release `kerr_worldline` orbit, infall, and negative-spin pair examples export 203, 41, and 407
rows respectively. All numeric CSV cells are finite and both clocks increase within each observer.
The infall example terminates at `r=2M`, `tau=13.5737865173M`, `t_KS=16.5932111268M`; the two
negative-spin circular examples reach `tau=100M` with separate coordinate readings. Invalid spin,
non-finite/negative duration, interior starting radius, and unknown scenario return exit code 1.
All six prior null CLI scenarios passed. The [then-current CPU regression report](benchmarks/results/2026-09-25.md)
records unchanged work and numerical results at the CSV precision.

At that milestone, remaining validation included an independently sourced Kerr trajectory fixture, much longer orbit
stress runs, unit/momentum rescaling studies, degenerately grazing events, the past-ray horizon
cutoff, Linux CI, adaptive frame transport, accelerated observers, and all GPU/image comparisons.
The current continuation gate above supersedes this historical list.
