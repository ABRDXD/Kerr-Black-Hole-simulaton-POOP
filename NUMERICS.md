# Numerical Strategy

## Status

The CPU tracer uses **Dormand-Prince 5(4)** with FSAL, a bounded adaptive controller, and quartic
dense output. An independently implemented **eighth-order modified-midpoint extrapolation** method
provides endpoint references. A fixed RK4 baseline is measured in the benchmark. GPU production
selection and any alternate recovery strategy remain benchmark-dependent; no GPU integrator exists yet.

## State and derivative evaluation

Hamiltonian states use Cartesian Kerr-Schild `(x^mu,p_mu)`. The CPU evaluates inverse-metric spatial
partials with forward-mode automatic differentiation using three derivatives per scalar. The time
partial is zero by stationarity. The radius/profile formulas are scaled before differentiation;
the scale is held constant at each evaluation point, which is valid because the expression is
algebraically independent of that arbitrary positive scale. Derivatives are checked against
five-point finite differences and analytical Schwarzschild cases.

The first GPU geometry kernel now uses explicit analytic inverse-metric derivatives,
specified in `GPU_PHYSICS.md` and checked against CPU automatic differentiation.
The CPU derivative implementation is separately checked against finite differences
and analytic Schwarzschild values. This is pointwise geometry conformance, not a
validated GPU trajectory solver. Binary32 acceptance bounds and failure codes are
versioned with the packed ABI; CPU physics and tolerances remain unchanged.

Finite differencing is a validation oracle, not the production GPU derivative method.

## Candidate integrators

The initial CPU benchmark compares DOPRI5, eighth-order extrapolated midpoint, and fixed RK4. The broader candidate set remains:

1. Dormand-Prince 5(4) with FSAL, scaled embedded error control, rejection, and dense output. It is the leading throughput candidate and supports accurate within-step event location.
2. A higher-order adaptive CPU reference method such as DOP853 for stringent truth datasets.
3. A robust alternate method for pathological rays, initially an implicit midpoint/Gauss-Legendre candidate or a conservatively stepped high-order explicit method. The fallback will be adopted only if measurements show it recovers meaningful rays at acceptable cost.
4. Fixed-step RK4 as a benchmark baseline, not the presumed final production method.

Hamiltonian geodesics in general coordinates are not separable into the simple kinetic-plus-potential form assumed by common explicit symplectic schemes. Adaptive stepping can also defeat naive symplectic claims. No method will be called symplectic without demonstrating the applicable formulation.

## Error norm and controller

For an embedded step with estimated component error `e_i`, the tentative scaled norm is

```text
err = sqrt((1/N) sum_i [e_i / (atol_i + rtol_i max(|y_i|,|y_i_new|))]^2).
```

Acceptance requires `err <= 1`. Step changes use a bounded safety-factor controller with minimum and maximum growth, finite step limits, and a rejection limit. Coordinate and momentum components receive physically meaningful independent scales; one large coordinate value must not hide momentum error.

Current defaults (`M=1` examples) are `rtol=1e-10`, position `atol=1e-11`, momentum
`atol=1e-12`, initial step `0.02`, minimum step `1e-12`, maximum step `0.5`, and
100,000 attempts with at most 32 consecutive rejections. The controller uses
`0.9 err^(-1/5)`, bounded to `[0.2,5]`, and prevents growth immediately following a rejection.
Non-finite or invariant-failing trials halve the step. The final affine-bound step may be shorter
than the configured minimum; floating-point stagnation always terminates explicitly.

The eighth-order reference uses modified midpoint with 2, 4, 6, and 8 substeps, polynomial
extrapolation in squared substep size, and the difference from the sixth-order extrapolate.
Its adaptive benchmark controller uses exponent `1/7`. It has no dense output and is used for
fixed-affine endpoint comparisons, not event truth. Tests separately demonstrate fifth- and
eighth-order endpoint convergence against the exponential ODE.

Accuracy modes own tolerances and invariant limits. The visual performance governor may lower resolution or samples before changing these tolerances and may never alter reference-mode tolerances.

## Events and termination

Every ray ends with a typed reason:

- horizon capture;
- escape boundary with outward motion;
- disk surface intersection;
- configured scene boundary;
- step/work budget exhausted;
- non-finite state or derivative;
- singular-geometry approach;
- repeated rejection or minimum-step failure;
- invariant/constraint violation after recovery;
- explicit cancellation.

Event functions are bracketed across accepted steps. Thin-disk intersection and horizon/escape crossings use dense output plus a bounded root solver; linearly interpolating a long curved step is not sufficient for the reference path.

In the implemented DOPRI5 tracer, each spatial coordinate is quartic in the within-step fraction.
The disk equation `z=0` is therefore quartic. A constant Kerr radius `R` is the oblate surface
`(x^2+y^2)/(R^2+a^2)+z^2/R^2-1=0`, a polynomial of degree at most eight. Recursive derivative-root
isolation partitions `[0,1]` into monotone intervals, followed by bounded bisection. This detects
multiple crossings even when the accepted-step endpoints have the same sign. The earliest eligible
root wins; radial events require the correct crossing direction and disk events require the configured
annulus. Exactly coplanar rays and tangencies are not counted as disk crossings. Initial disk-plane
points are skipped to avoid an immediate self-intersection; starting at/inside the horizon has its
own classification. Affine root tolerance defaults to `1e-10` and is limited by machine precision.

Root accuracy is accuracy on the dense numerical curve, not a guarantee of global geodesic accuracy.
The final interpolated event state must also pass the constraint/invariant gate. Near multiple roots
or a capture separatrix, classification requires a tolerance-refinement study. Past-directed rays
approaching a horizon in the ingoing chart can exhaust numerical limits; a renderer-specific capture
policy and its cutoff convergence are still required before backward images can be claimed validated.

## Constraint and invariant monitoring

For null rays, diagnostics record at least absolute and scale-normalized Hamiltonian residuals. Useful normalization is based on the sum of magnitudes of the Hamiltonian contraction terms so the reported number remains interpretable when momenta are rescaled.

`E=-p_t`, `L_z=x p_y-y p_x`, and the appropriate null or massive Carter constant `Q` are monitored. `PHYSICS.md` derives
the axis-regular Cartesian expression for `Q`; no full Boyer-Lindquist coordinate transform is
required for this scalar. Reports distinguish local error estimates, global endpoint error,
constraint residual, and conservation drift.

Current acceptance requires normalized null residual at most `1e-7` and each scaled invariant drift
at most `1e-7`. The drift convention is `|I-I_initial|/max(1,|I_initial|)` in the configured
geometrized units. Thus the invariant threshold is tied to the chosen momentum and mass normalization;
it is not advertised as invariant under arbitrary unit rescaling. Rejection retains the last accepted
state. No momentum projection is applied. Invalid input returns a typed result with a zero finite
state; singular or non-finite derivatives are cleared before returning failure.

The timelike path reuses the same DOPRI5 driver and event polynomial isolation. Its independent
parameter is elapsed proper time with unit rest mass. It reports `|2H+1|/(1+sum |g^ab p_a p_b|)`
and also enforces the absolute unit-mass residual `|2H+1|<=min(constraint_limit,1e-7)` at initial,
accepted, and interpolated event states. This prevents a large coordinate momentum scale from
hiding a change in proper-time normalization. Future `dt/dtau>0` is checked at these boundaries.
The initial-data velocity constructor uses a stricter absolute normalization check of `1e-10`.
No mass-shell projection or axis reset is applied.

Frame construction validates absolute orthonormal residuals at `1e-9` and right-handed determinant
within `4e-9`. The coupled 20-component parallel-transport RHS is integrated by `trace_observer`.
Its local error is the maximum of separate RMS norms for the eight geodesic and twelve spatial-axis
components, so extra frame components cannot dilute trajectory error. The default axis absolute
tolerance is `1e-12`; the absolute frame-error gate may be tightened but never relaxed above `1e-9`.
Both complete trial endpoints and dense event endpoints must pass frame and timelike gates.
For coordinate targets, the remaining coordinate interval also caps the proper-time trial; the
clock-root fraction tolerance scales with its initial linear estimate. This avoids a fixed dense
root tolerance overwhelming tiny target intervals, including boosted observers. Final coordinate
steps, like final proper-time steps, may be below `minimum_step`; clocks are never snapped to a
target independently of the evolved state.
The final accepted state is retained on failure. Proper time is elapsed internally and published
with the initial absolute clock offset; unrepresentable clock advances fail explicitly.
Independent midpoint8 refinement and analytic holonomy validate orientation without silent repair.
Highly boosted frames that binary64 cannot resolve are classified failures.

`advance_stationary_observer` applies DOPRI5 to twelve Fermi-Walker axis components in coordinate
time. The connection/generator is constant for a fixed spatial position and evaluated once per
advance. Position and proper clock are analytic, using extended-precision elapsed-time accumulation.
Every published event must advance both clocks and satisfy the same frame validation. Work,
rejection, and step limits are bounded, and cancellation retains the last accepted event.

Session scheduling overrides per-trace duration targets to reach a shared absolute KS time. Since
`u^t=gamma sqrt(1+F)>=1`, the coordinate interval bounds the needed proper-time interval. Each
geodesic additionally checks E/Lz/Q against its birth values, preventing a restart from erasing
lifetime drift. Histories retain accepted finite events and do not duplicate restart callbacks.

## Failure recovery

The bounded policy is:

1. reject and reduce the adaptive step;
2. after a small configured rejection threshold, retry from the last valid state with the validated alternate strategy if enabled;
3. terminate with a detailed reason if the state still fails.

The last valid state is retained for diagnostics. Rendering receives an explicit invalid-ray token, never partially corrupted radiance.

## Precision

- CPU reference: IEEE binary64 by default; selected truth datasets may use long double or multiprecision after cross-platform capability checks.
- GPU visual path: binary32 state with algebraic scaling and compensated/high-low techniques only where benchmarks justify them.
- GPU precision escalation: optional binary64 or split precision for a bounded subset of rays, only after querying support and measuring RTX 3050 cost.
- Constants and formulas are represented so literals do not silently change precision between C++ and shader code.

State storage is binary64. CPU derivative algebra and selected contractions accumulate in `long double`
(64-bit mantissa on the tested LLVM-MinGW build; this may equal binary64 with other compilers).
GCC/Clang targets explicitly use `-fno-fast-math -ffp-contract=off`; MSVC uses `/fp:strict`.
Address and undefined-behavior sanitizer builds stop on detected errors.

## GPU workload control

Adaptive rays naturally diverge. Candidate scheduling includes per-ray active masks, compacted continuation queues between bounded integration batches, quantized step-size bins, and separate queues for rare fallback rays. These are performance hypotheses until timestamped benchmarks demonstrate a win.

## Numerical research notes

- GRay2 demonstrates Cartesian Kerr-Schild integration and reports convergence for null and timelike Kerr geodesics: <https://arxiv.org/abs/1706.07062>.
- OSIRIS uses a Hamiltonian formulation, multiple integration schemes, and Hamiltonian-constraint tracking: <https://doi.org/10.1140/epjc/s10052-022-10054-0>.
- DamoyY/KerrTrace provides an Apache-2.0 example of GPU Dormand-Prince 5(4) and dense event handling; it is audited but not copied: <https://github.com/DamoyY/KerrTrace>.

Benchmark conclusions and exact tolerances will be appended here rather than rediscovered.

The initial result is in `benchmarks/results/2026-09-24.md`. DOPRI5 needs fewer derivative evaluations
on the measured workloads; eighth-order extrapolation supplies substantially tighter endpoint references.
The shared maximum step cap affects this comparison. These CPU data do not establish a GPU winner.

Formula references: [Dormand-Prince tableau](https://ketch.github.io/numipedia/methods/DP5.html),
[SciPy RK45 coefficient reference at the audited revision](https://github.com/scipy/scipy/blob/92d2a8592782ee19a1161d0bf3fc2241ba78bb63/scipy/integrate/_ivp/rk.py),
[modified-midpoint recurrence](https://xmds2.readthedocs.io/en/latest/bulirschStoer.html).
