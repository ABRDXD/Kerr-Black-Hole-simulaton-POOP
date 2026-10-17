# Physics Conventions and Model

## Scope and units

The primary spacetime is uncharged Kerr. Internally `G = c = 1`; a mass parameter therefore has dimensions of length and time. Production sessions normally normalize the black-hole mass to `M = 1`, while the configured physical mass supplies display scales `GM/c^2` and `GM/c^3`.

The metric signature is `(-,+,+,+)`. Greek indices run over spacetime coordinates. Coordinate component order is always stated in APIs and packed buffers.

The spin parameter `a = J/M` has the same geometrized dimension as `M`, with `|a| <= M`. The dimensionless spin is `chi = a/M`. Initial versions allow the mathematically extremal endpoint, but production presets stop below it because near-extremal ray dynamics are a stringent numerical regime.

## Primary coordinates: ingoing Cartesian Kerr-Schild

Primary integration coordinates are `x^mu = (t,x,y,z)`. Define the implicit oblate radius `r >= 0` by

```text
r^4 - r^2 (x^2 + y^2 + z^2 - a^2) - a^2 z^2 = 0.
```

The implementation solves the quadratic for `r^2` with a cancellation-aware expression and `hypot`. The ring `r = 0, z = 0, x^2+y^2=a^2` is a physical curvature singularity and is returned as a classified invalid geometry, not a NaN.

With `eta_mu_nu = diag(-1,1,1,1)`, define

```text
l_mu = (1,
        (r x + a y)/(r^2+a^2),
        (r y - a x)/(r^2+a^2),
        z/r)
F = 2 M r^3 / (r^4 + a^2 z^2).
```

This fixes KerrLab's spin orientation and ingoing chart convention. The Kerr-Schild covector is null with respect to `eta`: `eta^{mu nu} l_mu l_nu = 0`. Raising it with `eta` gives `l^mu = (-1,l_x,l_y,l_z)`. The metric and exact inverse are

```text
g_mu_nu = eta_mu_nu + F l_mu l_nu
g^mu_nu = eta^mu_nu - F l^mu l^nu.
```

For this rank-one null update, `det(g) = det(eta) = -1`. Tests verify nullness, inverse consistency, determinant, symmetry, and the Schwarzschild/asymptotic limits. This chart crosses the future event horizon without the Boyer-Lindquist `Delta = 0` coordinate singularity.

## Hamiltonian and momenta

The headless binary32 GPU implementation now evaluates these same Kerr-Schild
metrics and their analytic spatial inverse-metric derivatives. `GPU_PHYSICS.md`
specifies its scaled derivative algebra and packed ABI; the 2026-10-07 report
records pointwise CPU comparisons. This does not yet implement GPU worldlines,
camera integration, or a past-ray horizon capture policy.

Canonical momenta are covariant components `p_mu`. For an affinely parameterized geodesic,

```text
H(x,p) = 1/2 g^{mu nu}(x) p_mu p_nu
dx^mu/dlambda = g^{mu nu} p_nu
dp_mu/dlambda = -1/2 (partial_mu g^{alpha beta}) p_alpha p_beta.
```

Null geodesics satisfy `H = 0`; timelike geodesics parametrized by proper time and normalized to unit rest mass satisfy `H = -1/2`. Backward rendering constructs a past-directed null four-momentum in the observer tetrad and transforms it to the coordinate basis before lowering its index.

Stationarity conserves `E = -p_t`. Axisymmetry conserves `L_z`; in Cartesian canonical variables,
`L_z = x p_y - y p_x`. The null Carter constant is evaluated by the axis-regular formula below.
No integrator should enforce a drifting invariant by silently projecting a physically different
trajectory; any optional constraint projection must be an explicit, measured numerical method.

### Null Carter invariant in Cartesian coordinates

At fixed oblate radius and Kerr-Schild azimuth, `p_theta = cot(theta)(x p_x+y p_y)-r sin(theta)p_z`.
The theta momentum is unchanged by the radial time/azimuth shifts to Boyer-Lindquist coordinates.
Starting from `Q=p_theta^2+cos(theta)^2[L_z^2/sin(theta)^2-a^2 E^2]`, use
`z=r cos(theta)`, `x^2+y^2=(r^2+a^2)sin(theta)^2`, and
`(x p_x+y p_y)^2+L_z^2=(x^2+y^2)(p_x^2+p_y^2)` to obtain

```text
s = 1 + a^2/r^2
Q = z^2 s (p_x^2+p_y^2) - 2 z (x p_x+y p_y)p_z
    + (x^2+y^2)/s p_z^2 - (a z/r)^2 E^2.
```

This expression is continuous on the rotation axis for `r>0`. Timelike unit-rest-mass diagnostics
add `(a z/r)^2`, so the last term becomes `(a z/r)^2 (1-E^2)`. The mass is fixed to unity,
not inferred from a drifting Hamiltonian. Tests check the Schwarzschild angular-momentum limit,
axis values, circular equatorial photon orbits, and conservation on generic Kerr trajectories.

`make_null_state` supplies scientific initial data from spatial covariant momentum: it solves the
null quadratic for the future-directed `p_t` root, using a cancellation-resistant alternate
expression. Its past-directed option negates all four momentum components after construction.
This helper does not interpret its three inputs as a physical camera direction; that requires
`make_camera_ray` and the observer-tetrad implementation below.

## Horizons and stationary limit

The Kerr radial horizon values are

```text
r_plus/minus = M +/- sqrt(M^2-a^2).
```

The outer stationary-limit surface is

```text
r_ergo(theta) = M + sqrt(M^2-a^2 cos^2(theta)).
```

These are values of the Kerr oblate radius, not ordinary Euclidean sphere radii in the Cartesian chart.
The CPU tracer locates inward horizon crossings for future-directed rays and can continue inside
when this termination is disabled. Past-directed rays in an ingoing chart need a separately validated
near-horizon capture prescription: they need not cross the future horizon at finite chart time.
That renderer policy is not yet implemented. The horizon is neither emissive nor a material surface.

## Equatorial circular landmarks

The CPU foundation implements the standard Bardeen-Press-Teukolsky algebraic ISCO radius. `prograde` and `retrograde` mean relative to the black-hole spin and therefore use `|chi|` for the radius; coordinate angular-velocity signs still depend on the signed spin and orbital orientation.

For `q = |a|/M`,

```text
Z1 = 1 + cbrt(1-q^2) [cbrt(1+q) + cbrt(1-q)]
Z2 = sqrt(3q^2 + Z1^2)
r_ISCO/M = 3 + Z2 - s sqrt((3-Z1)(3+Z1+2Z2)),
```

where `s=+1` for prograde and `s=-1` for retrograde. Equatorial circular photon radii used as validation landmarks are

```text
r_ph,pro/M   = 2 [1 + cos((2/3) acos(-q))]
r_ph,retro/M = 2 [1 + cos((2/3) acos(+q))].
```

The full Kerr photon region is not a spherical shell; later visualizations will sample the spherical null-orbit family rather than label a single photon-sphere radius for nonzero spin.

## Observer events and camera rays (implemented CPU)

An observer event contains position `x^mu`, future-directed unit four-velocity `u^mu`, proper time, coordinate time, and an oriented orthonormal tetrad `e_(a)^mu` satisfying

```text
g_mu_nu e_(a)^mu e_(b)^nu = eta_(a)(b),  e_(0)^mu = u^mu.
```

Camera pixel directions live in this local Minkowski frame. Arbitrary velocities are accepted only when timelike and normalized. Stationary observers are unavailable inside the stationary-limit surface; the UI must report this physical restriction instead of clamping to a fake state.

Multiple observers own independent worldlines and clocks. Comparing their readings requires an explicitly defined exchange of signals or coordinate construction; raw proper times are not treated as interchangeable.

`ObserverEvent` stores `(t,x,y,z)`, a proper-clock reading, and four contravariant basis vectors.
The temporal vector is the four-velocity. Factories support the Eulerian normal, a physical local
velocity relative to that normal, fixed Cartesian coordinates, and equatorial circular geodesics.
`make_observer` accepts an already unit future-timelike velocity without silently renormalizing it.
Fixed-coordinate observers require `g_tt<0`; general observers may exist inside the ergoregion and
across the future horizon. Since `g^tt=-(1+F)<0`, the future direction is `u^t>0`.

The symmetric Cartesian Eulerian frame avoids spherical-axis degeneracies. With `s=sqrt(1+F)`,

```text
n^mu = (s, -F l_i/s)
E_(i)^t = 0
E_(i)^j = delta_ij - F/[s(s+1)] l_i l_j.
```

For an arbitrary observer let `w_i=g(u,E_i)` and `gamma=u^t/s`. Its axes are the rotation-free
Lorentz boost `e_i=E_i+w_i n+w_i/(gamma+1) sum_j w_j E_j`. This is an initial orientation choice,
not a rule for resetting axes while the observer moves. Validation checks finite data, future
direction, absolute orthonormal residual at most `1e-9`, and determinant within `4e-9` of `+1`.
Extremely ill-conditioned frames fail explicitly.

Circular observers require exactly `z=0`, a radius outside the horizon and selected photon orbit,
and a timelike tangent. They include unstable circular orbits below the ISCO. For coordinate
rotation sign `d=+/-1`, `Omega=d sqrt(M)/(r^(3/2)+d a sqrt(M))`, with
`u` proportional to `(1,-Omega y,Omega x,0)`. Prograde/retrograde is relative to signed spin;
for zero spin prograde selects positive rotation. This follows Bardeen, Press and Teukolsky (1972),
[Eq. 2.16](https://articles.adsabs.harvard.edu/pdf/1972ApJ...178..347B).

For a normalized local spatial direction `n`, the camera uses `k=nu(u+n)` for future propagation
and `k=nu(-u+n)` for past propagation. The same input direction therefore keeps the spatial ray
direction fixed; negating an entire ray also requires negating `n`. Frequency is positive and is
measured as `-p_mu u^mu` for future momenta or `+p_mu u^mu` for past momenta. The caller must supply
the correct orientation. This differs intentionally from `make_null_state`, whose past option
negates the whole momentum. Past-ray horizon termination remains an unvalidated renderer policy.

## Timelike worldlines and orientation (implemented CPU)

`make_timelike_state` lowers a supplied future unit four-velocity. `trace_timelike_worldline`
uses the Hamiltonian solver with unit rest mass, `H=-1/2`, and the massive Carter constant above.
Its increasing integration parameter is elapsed proper time; `state[0]` is the ingoing KS coordinate
clock. Callbacks expose independent accepted worldline samples and both clock readings. The
`kerr_worldline --scenario pair` example traces two independent circular worldlines; it does not
simulate exchanges of clock signals or define simultaneity beyond the supplied coordinates.

The Schwarzschild infall example has energy per unit mass `E=1`, zero angular momentum, and
`w=sqrt(2M/r)`, `u^r=-w`, `u^t_KS=1+w^2/(1+w)`. This remains finite at the future horizon.
Its proper-clock oracle is `r(tau)=[r0^(3/2)-(3/2)sqrt(2M) tau]^(2/3)`.

`TransportState` couples the eight Hamiltonian components to three spatial frame vectors.
`parallel_transport_rhs` evaluates `de_i^mu/dtau=-Gamma^mu_ab u^a e_i^b`; the temporal leg is
recovered from the momentum. Covariant metric derivatives follow `partial g=-g(partial g^-1)g`.
Unpacking validates the transported frame without projecting it back onto a different orientation.
The general transport equations are described in [Faye and Seraj, section 2](https://arxiv.org/html/2409.02624v2).
The derivative primitive is tested with the independent eighth-order integrator, including the
analytic Schwarzschild circular gyroscope holonomy. `trace_observer` now integrates this coupled
state adaptively, checks absolute mass-shell and frame errors, and locates horizon, escape, disk,
and coordinate-clock events on the dense curve. Its clock starts at the supplied observer's proper
time; restarting uses the transported axes without resetting their orientation.

Fixed-coordinate accelerated observers use `stationary_kinematics` and
`advance_stationary_observer`. They require `g_tt<0`, with

```text
u^mu = (1/sqrt(-g_tt),0,0,0)
a^mu = Gamma^mu_tt (u^t)^2
tau(t) = tau0 + sqrt(-g_tt) (t-t0)
D e_i^mu/dtau = u^mu (a.e_i) - a^mu (u.e_i).
```

The full antisymmetric Fermi-Walker generator acts on all intermediate axes; its coefficients
are constant along a stationary worldline. The adaptive driver evolves the axes in coordinate
time and validates the frame without projection. In Schwarzschild the proper acceleration is
`M/[r^2 sqrt(1-2M/r)]`. Arbitrarily prescribed accelerated trajectories remain future work.
The Fermi-Walker sign and equatorial Kerr precession oracle follow
[Kocherlakota et al. (2019), equations (2) and (38)](https://academic.oup.com/mnras/article/490/3/3262/5568376).
For a fixed equatorial observer the signed proper-time precession vector is
`-a M/[r^2(r-2M)]` along Cartesian `+z`.

`ObserverSession` advances active observers to a shared ingoing KS time slice, with independent
proper clocks and orientation histories. A captured observer retains its earlier horizon event
while other observers continue. This is a coordinate simultaneity convention, not a simulation of
signal exchange. `kerr_session` exports all four tetrad legs; `kerr_worldline` still exports only
trajectory and clock diagnostics.

## Accretion disk and radiation (designed)

The first physical disk is an optically thick, geometrically thin Novikov-Thorne-type disk with a zero-torque inner boundary as a selectable baseline. The model defines surface flux, effective temperature, circular four-velocity, inner edge (normally the appropriate ISCO), outer edge, mass accretion rate, and spectral emissivity.

For a photon with four-momentum `k_mu`, the invariant frequency ratio is

```text
g = nu_observed / nu_emitted
  = (k_mu u_observer^mu) / (k_mu u_emitter^mu).
```

Specific intensity obeys the invariant `I_nu/nu^3`; bolometric intensity from an unresolved surface acquires the corresponding integrated frequency factor. The renderer will not interchange `g^3` and `g^4` without stating whether it transports monochromatic or bolometric intensity.

Disk turbulence, bloom, exposure, tone mapping, and exaggerated diagnostic colors are presentational layers. They are never labeled as predictions of the Kerr metric.

## Physical constants used for display

The unit conversion module currently uses `c = 299792458 m/s` exactly, `G = 6.67430e-11 m^3 kg^-1 s^-2`, and a conventional solar mass `M_sun = 1.98847e30 kg`. These values affect human-readable scaling, not dimensionless geodesics.

## Coordinate transforms (planned)

Boyer-Lindquist and Kerr-Schild diagnostic transforms will be added with explicit branch and azimuth conventions. They will be round-trip tested away from known degeneracies. The implementation will not infer a Boyer-Lindquist azimuth using an ordinary spherical `atan2(y,x)` because Kerr-Schild azimuth includes a spin-dependent shift.

## References

- B. Carter, *Global Structure of the Kerr Family of Gravitational Fields* (1968).
- J. M. Bardeen, W. H. Press, S. A. Teukolsky, *Rotating Black Holes: Locally Nonrotating Frames, Energy Extraction, and Scalar Synchrotron Radiation* (1972).
- R. P. Kerr and A. Schild, *Some Algebraically Degenerate Solutions of Einstein's Gravitational Field Equations* (1965).
- C. W. Misner, K. S. Thorne, J. A. Wheeler, *Gravitation* (1973).
- S. Chandrasekhar, *The Mathematical Theory of Black Holes* (1983).

// refactor: polish PHYSICS.md implementation details

// refactor: polish PHYSICS.md implementation details

// refactor: polish PHYSICS.md implementation details

// refactor: polish PHYSICS.md implementation details

// refactor: polish PHYSICS.md implementation details

// refactor: polish PHYSICS.md implementation details

// refactor: polish PHYSICS.md implementation details

// refactor: polish PHYSICS.md implementation details
