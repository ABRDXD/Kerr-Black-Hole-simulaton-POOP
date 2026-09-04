# Architecture

## Current desktop delivery and native target

As of 2026-10-12, the working desktop app uses `web-demo/desktop/main.cjs`
(Electron 44.7.0) to load bundled `dist/index.html` with the imported WebGL 2
renderer. The optional WebGPU still renderer is another separate shader path.
The shell keeps Node integration off and renderer sandbox/context isolation on;
there is currently no C++ worker bridge. The native C++ core and Vulkan
diagnostic remain independently implemented and tested subsystems.

The diagram and native module boundaries below describe the scientific target,
not a claim that the current desktop image already uses Vulkan. `ROADMAP.md`
defines the staged scene-state/data bridge and native image integration work.
Before connecting engines, explicitly adapt units, spin, component ordering,
ray direction, observer frames, clocks and session-generation/reset semantics.
Preserve the usable desktop window while those contracts are implemented.

## Design constraints

The native target has CPU and GPU physics implementations with different jobs. The CPU reference favors auditability and accuracy. The GPU path favors throughput under documented tolerances. Neither is allowed to invent a second set of conventions. Imported renderer implementations require explicit adapters and independent validation before they can share these guarantees.

```text
configuration / commands
          |
          v
simulation session -----> immutable Kerr spacetime
          |                         |
          v                         v
 observers/worldlines         geometry + units
          |                         |
          +------> geodesic systems + integrators
                                  |
                     +------------+-------------+
                     |                          |
               CPU reference             GPU representation
                     |                          |
               validation <------------- Vulkan compute rays
                                                |
                                    radiative transfer / scene
                                                |
                                  accumulation / reconstruction
                                                |
                                   presentation + instrumentation
```

## Target layout

```text
include/kerr/
  math/               small explicit mathematical types
  physics/            metrics, coordinates, observers, disks, units
  numerics/           integrators, error norms, dense output, roots
  diagnostics/        typed counters, histories, and failure records
src/
  physics/            high-precision CPU reference implementation
  numerics/           CPU integrators and convergence tools
  gpu_physics/         packed layouts and conformance-vector generation
  renderer/vulkan/     Vulkan ownership, synchronization, pipelines
  scene/               environments, disk render state, trajectories
  instrumentation/     aggregation, plots, and export
  ui/                  cinematic/scientific/hybrid panels
  app/                 simulation orchestration
  platform/            Windows/Linux integration only
shaders/               Vulkan compute and presentation shaders
tests/                 deterministic unit and regression tests
benchmarks/            reproducible CPU/GPU workloads and results
config/                human-readable schemas and presets
```

The `math`, `physics`, and header-only `numerics` slices now exist, together with CPU
command-line applications, physical observer frames, null and timelike tracing, observer sessions,
regression tests, and an integrator benchmark. `gpu_physics` now owns the v1 packed
metric records, deterministic fixtures and narrow SPIR-V ABI inspector. The
`renderer/vulkan` owns the shared context and diagnostic presentation resources;
`platform` owns the GLFW window. The scientific UI remains designed work.

`GeodesicState` is the explicit eight-component `(x^mu,p_mu)` state. `geodesic_rhs` computes
Hamilton's equations and clears its output on failure. The numerical steppers accept an abstract
derivative callable and do not include physics headers. `trace_null_ray` owns adaptation, physical
event functions, conservation gates, and typed termination. Its optional observer callback receives
only accepted finite states, allowing CSV export without coupling the solver to file I/O.

`trace_timelike_worldline` shares the null solver's adaptation and event machinery, with unit-mass
constraint and Carter diagnostics. Its callback parameter is elapsed proper time; coordinate time
remains `state[0]`. `ObserverEvent` is a checked value snapshot, not a mutable simulation owner.
`Observer` constructs local frames and camera rays; `Transport` supplies a coupled geodesic/axis
RHS for geodesic parallel transport. `trace_observer` integrates all 20 components with bounded
adaptation, dense stopping events, and independent frame-error gates. `Connection` shares the
contracted connection calculation with `StationaryObserver`, which evolves Fermi-Walker axes at
fixed Cartesian coordinates and evaluates its clocks analytically.

`kerr_trace` constructs documented null initial data and exports scientific trajectories.
`kerr_worldline` exports orbit, radial infall, and paired independent observer clocks. `kerr_session`
exports geodesic and stationary observers on shared coordinate-time slices, including transported
tetrads and bounded individual histories. `kerr_benchmark`
compares endpoint integration methods independently of event-location and presentation costs.

## Ownership and data flow

`ObserverSession` is the implemented single-threaded CPU owner of immutable spacetime parameters,
a requested KS coordinate-time slice, and independently evolving geodesic/stationary observers.
Histories have bounded capacity with visible drop counts; snapshots and histories are copies.
An event or numerical failure freezes only the affected observer. Geodesic invariant checks retain
their birth baseline across scheduling intervals. Advancing works on candidate entries before
committing, so a configuration exception leaves the session unchanged. Reset replaces parameters,
clears observers/histories, and increments the ID generation so old IDs cannot resume.

The future `SimulationSession` will extend this ownership to particles, reproducibility descriptors,
typed UI commands, GPU histories, and temporal accumulation. Those rendering resources must also
be invalidated when spacetime parameters change.

The renderer reads immutable per-frame snapshots. It does not reach into live physics state. GPU buffers use versioned packed structs with compile-time size/alignment checks and shader reflection checks.

Diagnostic counters remain on the GPU during a frame and are reduced asynchronously. UI reads a delayed snapshot to avoid forcing a CPU/GPU synchronization point.

## Dependency policy

- Physics and numerics are usable in a command-line validation executable with Vulkan disabled.
- GPU structures may depend on physics value types, never the reverse.
- UI depends on application-facing snapshots and commands, not metric implementations.
- Platform code owns native handles and converts them to renderer-neutral surface creation inputs.
- Configuration parsing produces validated domain objects; no unvalidated stringly typed settings enter physics hot paths.

## Threading model (designed)

- Main thread: event pump, commands, UI composition.
- Simulation worker: observer/particle integration and snapshot publication.
- Render thread: Vulkan submission and presentation.
- Background workers: CPU reference jobs, validation, asset decoding, and report export.

Snapshots cross threads through bounded queues. Scientific reference jobs may consume substantial time but must not stall the UI event pump.

## Vulkan direction (designed)

The implemented headless slice dynamically loads the system Vulkan runtime, requires
device/loader API 1.3 and synchronization2, chooses a compute queue, and prefers a
suitable discrete GPU unless an enumeration index is supplied. Debug validation is
enabled when available; strict conformance runs require it. Capabilities and missing
requirements are printed explicitly. No Vulkan type escapes the ComputeContext API.

For validation, each dispatch owns two mapped storage buffers, descriptors, pipeline,
command pool, fence and optional timestamp queries. Host-visible memory supports both
coherent and flush/invalidate paths. A shader-write to host-read barrier and fence order
readback. Resources are released by RAII, including partial initialization failures.
Empty/oversized dispatches fail before allocation; an application ceiling bounds the
group count independently of driver-advertised limits. This synchronous allocation
strategy is a conformance foundation, not the persistent asynchronous production renderer.

`kerr_gpu` composes physics references, GPU fixtures and the Vulkan context. The Vulkan
library does not link physics; CPU physics does not include Vulkan. The actual SPIR-V
descriptor/type/layout graph is checked against the packed contract before dispatch.
No generic reflection library or third-party Vulkan dispatch code is incorporated.

The window slice shares the internal `VulkanContext` with headless compute.
`platform::Window` owns GLFW and supplies an opaque `SurfaceSource`; it must
outlive `WindowRenderer`. Events are polled on the owning thread, including
between bounded fence waits. Renderer code does not include GLFW; physics
still has no Vulkan/platform dependency.

The window requires a combined graphics/compute/present queue, API 1.3,
synchronization2, KHR surface maintenance and KHR swapchain maintenance1.
Missing requirements fail explicitly. One frame is in flight. Pipeline,
descriptors, command pool and image storage persist until resize. Only explicit
diagnostic captures perform readback. Compute writes RGBA8, blitted into the
FIFO swapchain. Render and presentation fences protect reuse and destruction.
Presentation fences establish resource release, not proof of on-screen display.

This is diagnostic presentation infrastructure. GPU geodesic rendering, HDR
conversion, asynchronous histories and multi-frame scheduling remain future
work; the threaded model above is not yet implemented.

## Configuration

The initial human-readable format will be TOML with a versioned schema. Configurations separate immutable spacetime creation, observer state, disk physics, rendering, numerical tolerances, environment, quality governor, and deterministic validation settings. Unknown or invalid fields are reported with paths and accepted ranges.
