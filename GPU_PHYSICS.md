# GPU metric conformance, formula and ABI version 1

The first GPU kernel evaluates Kerr geometry and analytic inverse-metric spatial
derivatives in binary32. It does not integrate rays. The CPU reference still uses
binary64 and automatic differentiation. Physical conventions are in PHYSICS.md.

## Buffer contract

Descriptor set 0 contains two std430 storage buffers, binding 0 read-only inputs
and binding 1 write-only outputs. Each buffer has one runtime array at offset 0.
The shader uses local size (64,1,1), and bounds invocations by input-array length.
There are no push constants, matrices with implicit major order, or scalar-layout
extensions. The C++ declarations are in MetricLayout.hpp.

Input stride is 48 bytes: vec4 (x,y,z,M) at 0; vec4 (a,0,0,0) at 16; uvec4
(schema=1,case ID,0,0) at 32. Output stride is 352 bytes: uvec4 (schema=1,case ID,
status,0) at 0; vec4 (r,F,0,0) at 16; float covariant[16] at 32; float inverse[16]
at 96; float partials[48] at 160. Scalar arrays have stride 4. Matrices flatten
as row*4+column; partials flatten as axis*16+row*4+column with axes x,y,z.
The t partial is exactly zero by stationarity and is not stored.

Status codes: 0 valid, 1 invalid input/parameters/reserved fields, 2 degenerate
chart, 3 unsupported schema, 4 numerical failure. Every failure has a completely
zero numeric payload. Version and case ID are echoed even on failure. No clamping
of spin, metric values, or failed derivatives is allowed. Floating overflow or
underflow that destroys the formula is reported, not a claim of arbitrary-range
binary32 support.

Compile-time C++ layout assertions and SPIR-V ABI reflection check the actual
shader's descriptor graph, offsets, scalar types, array lengths and strides, and
local size before dispatch. Reflection is deliberately limited to this contract;
it is not a validator for untrusted arbitrary SPIR-V.

The headless dispatcher separately enforces a 16,384-workgroup application ceiling
before allocation/submission. Device-advertised limits alone do not provide a
practical work budget. The conformance runner also checks partial/exact batches
of 1/63/64/65 records and a reversed full batch.

## Analytic derivatives

Let s=max(|x|,|y|,|z|,|a|), and X=x/s, Y=y/s, Z=z/s, A=a/s, m=M/s.
Freeze s when differentiating, as in the CPU automatic-derivative formula.
q=X^2+Y^2+Z^2-A^2 and D=sqrt(q^2+4 A^2 Z^2).
Use R^2=(q+D)/2 for q>=0, or 2 A^2 Z^2/(D-q) for q<0, and R=sqrt(R^2).
Then physical r=s R and

```text
grad R = (R X/D, R Y/D, Z (R^2+A^2)/(R D))
c=A Z/R^2; F=2m/[R(1+c^2)]
d_j c=A delta_jz/R^2 - 2 c d_j R/R
d_j F=-F [d_j R/R + 2c d_j c/(1+c^2)]
B=R^2+A^2; d_j B=2 R d_j R
l=(1,(R X+A Y)/B,(R Y-A X)/B,Z/R)
d_j l_x=(X d_j R+R delta_jx+A delta_jy-l_x d_j B)/B
d_j l_y=(Y d_j R+R delta_jy-A delta_jx-l_y d_j B)/B
d_j l_z=(delta_jz-l_z d_j R)/R
```

Raise l with eta=(-1,1,1,1). Compute g and inverse from PHYSICS.md; the inverse
partial is `-[d_j F L^mu L^nu + F(d_j L^mu L^nu+L^mu d_j L^nu)]/s`.
Mirror the upper triangle for exact symmetry. The shader marks arithmetic precise
to inhibit contraction/reassociation and is compiled without optimizer passes.
This does not promise CPU/GPU bitwise equality or correctly rounded square roots.

## Conformance gate

CPU references are evaluated at the exact uploaded binary32 inputs promoted to
double, so input quantization is separate from shader arithmetic error. Fixtures
cover signed/extremal spin, horizons/interiors, axis, equator, q<0, weak field,
mass scales 1e-6 through 1e6, deterministic off-axis samples, invalid data, schema
mismatch, and degeneracy. Acceptance bounds, fixed before GPU measurement:

- r: 3e-5 M + 3e-5 |r_ref|;
- F and metric components: 3e-5 + 3e-5 |reference|;
- partials: 3e-5/M + 3e-4 |reference|.

Reports include raw absolute errors and error divided by each acceptance bound.
All numeric fields must be finite; failed results must be entirely zero. These
are pointwise geometry bounds on the tested fixtures, not trajectory or image
accuracy guarantees. Near-ring resolution and broader exponent ranges require
additional studies before production use.

// refactor: polish GPU_PHYSICS.md implementation details

// refactor: polish GPU_PHYSICS.md implementation details

// refactor: polish GPU_PHYSICS.md implementation details
