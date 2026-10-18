# Performance and Adaptive Quality

The current Electron/WebGL desktop app and the native Vulkan track have
separate performance evidence. The desktop's prior 31–33 fps UI observation
is not a controlled benchmark or a minimum frame rate. A repeatable desktop
baseline and automatic quality controller are ROADMAP D0.9 and D8 work;
the policy below describes their intended measurement/quality boundaries.

The [2026-10-07 continuation report](benchmarks/results/2026-10-07.md) records
the first headless Vulkan metric/derivative conformance on the Radeon 740M and
RTX 3050, with synchronization validation. Its single cold dispatch timestamps
are diagnostics, not a throughput comparison. Persistent pipelines, asynchronous
readback and an actual GPU ray workload are still needed for rendering benchmarks.
All 18 CPU integrator rows retain their prior numerical/work columns.

## Hardware target

The primary tuning target is an NVIDIA RTX 3050 laptop GPU, where memory bandwidth, power limits, thermal state, and weak FP64 throughput matter. Correctness gates are fixed first; optimization follows timestamped profiling on representative scenes.

## Measurement policy

Every substantial optimization records:

- hardware, driver, power mode, build revision, compiler and flags;
- scene/configuration and deterministic seed;
- internal and output resolution;
- numerical mode and tolerances;
- median, p95, and p99 CPU/GPU frame times after warm-up;
- ray throughput, accepted/rejected steps, work distribution, and termination counts;
- image/trajectory error against a reference.

Claims without this context are hypotheses. The CPU reference never uses fast-math.

## GPU strategy

- structure data for coalesced access and explicit alignment;
- keep per-ray state and counters on device;
- process integration in bounded batches so pathological rays do not monopolize a dispatch;
- reduce diagnostic counters on the GPU and read them asynchronously;
- reuse descriptor sets, command buffers, and images where practical;
- use timestamp queries around individual passes;
- inspect synchronization with validation layers and a GPU profiler;
- adopt compaction, subgroup operations, or step bins only after measuring divergence and traffic.

## Three independent quality axes

1. **Numerical accuracy:** integration tolerance, precision policy, event-location tolerance, derivative mode, invariant limits. Reference mode locks this axis.
2. **Visual sampling:** internal resolution, samples per pixel, disk/volume samples, environment filtering, temporal history, presentation effects.
3. **Diagnostic cost:** heatmaps, per-ray histories, convergence pairs, readback frequency, and developer traces.

A single quality slider must not blur these categories.

## Performance governor

The governor consumes moving-window GPU timings and workload diagnostics. It uses hysteresis and rate limits to avoid oscillation. Decisions are explicit events shown in the UI and capture metadata.

Tentative degradation order in cinematic/hybrid modes:

1. reduce secondary/environment supersampling;
2. reduce expensive volumetric or spectral sampling;
3. reduce internal resolution;
4. shorten temporal histories when motion invalidates them;
5. reduce optional diagnostic layers/readback cadence;
6. change the visual integration budget only within the selected, documented numerical mode.

Scientific reference mode disables automatic changes to numerical accuracy. If its budget is exceeded, it runs slowly and reports the overrun.

## Benchmark suites (planned)

- flat/asymptotic rays;
- Schwarzschild shadow grid;
- moderate-spin inclined disk;
- near-extremal prograde photon-region stress case;
- moving/free-fall observer;
- invalid-state and minimum-step stress set;
- native versus reconstruction image comparisons;
- CPU/GPU derivative and integration agreement.

Results will be committed under `benchmarks/results/` as small machine-readable summaries, not raw multi-gigabyte captures.

## First CPU baseline (2026-09-24)

The reproducible `kerr_benchmark` tool and [initial results](benchmarks/results/2026-09-24.md)
compare adaptive DOPRI5, adaptive eighth-order modified-midpoint extrapolation, and fixed RK4.
Three deterministic finite-affine workloads cover weak field, inclined strong Kerr, and near-extremal
spin. The report contains all initial data through the source-defined cases and records endpoint
errors, constraint errors, derivative work, and median times over seven warmed runs.

DOPRI5 is the initial CPU tracing choice because it combines lower measured derivative work with
dense event output. The eighth-order method remains the endpoint reference. This baseline excludes
event isolation, CSV writing, rendering, and GPU work; its throughput cannot be read as frame rate.
Maximum step `0.5M` caps both adaptive methods and affects the comparison. No automatic recovery
method or GPU integrator is selected by these data.

The [2026-09-25 regression run](benchmarks/results/2026-09-25.md) after the observer/timelike
additions preserves all 18 rows' work and numerical results at reported precision. Tight-row
median timings are 1.001–1.044 times the prior values in the same short, uncontrolled setup;
no speedup or stable slowdown is claimed. Observer, timelike event, and transport performance
need separate workloads before any throughput claim is made for those features.

The [2026-09-28 continuation regression](benchmarks/results/2026-09-28.md) also preserves all
18 rows' work and numerical results. Adaptive orientation transport and observer sessions
are now implemented, but the null integrator benchmark still does not measure their throughput.
