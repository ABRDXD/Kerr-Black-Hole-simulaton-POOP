# Kerr-Black-Hole simulaton

> *VIbeCoded btw so dont actually try to copy this use anything else*  
> *Interactive relativistic Kerr spacetime ray-tracer, geodesic numerical integrator, and accretion disk simulator.*  
> *Features full Boyer-Lindquist metric formulation, Carter constant invariants, Doppler beaming, gravitational redshift, WebGL 2 / WebGPU compute, and native C++20 Vulkan pipelines.*

---

## Overview

**Kerr-Black-Hole simulaton** is an end-to-end relativistic astrophysics framework and real-time visualization system for spinning (Kerr) black holes. It simulates light propagation along null geodesics in curved spacetime, accretion disk thermodynamics, Doppler boosting, gravitational redshift, and frame-dragging in the ergosphere.

Key capabilities:
- **Exact Spacetime Physics**: Complete Boyer-Lindquist metric tensor $g_{\mu\nu}$, Christoffel symbols, Riemann curvature, and Carter constant of motion $Q$.
- **High-Order Geodesic Integration**: 4th-order Runge-Kutta (RK4) and adaptive Dormand-Prince 8(5,3) (DOP853) integrators preserving energy and angular momentum to $< 10^{-12}$ relative drift.
- **Relativistic Optics**: Backward ray tracing from camera observer, frequency shift ratio $g = (u_\mu k^\mu)_{obs} / (u_\mu k^\mu)_{emit}$, and relativistic beaming ($I_\nu \propto g^3$).
- **Dual Computation Backends**:
  - **Native C++20 / Vulkan Engine**: GPU compute shaders (`metric_v1.comp`) and headless CPU batch ray marching.
  - **Interactive WebGL2 / WebGPU Desktop**: Full standalone application powered by Electron 44 with orbit navigation, relativistic accretion disks, and binary merger visualizations.
- **Cinematic Movie Exporter**: Deterministic high-resolution frame sequence export at 60 or 144 FPS.

---

## Quick Start

### 1. Launch Standalone Desktop App (Linux / Arch)

```bash
# Verify environment and launch
./start-kerrlab-arch.sh --check
./start-kerrlab-arch.sh
```

### 2. Launch on Windows

```powershell
.\start-black-hole.ps1
```

### 3. Build Native C++20 Benchmarks & Physics Engine

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

---

## Architecture

| Component | Path | Description |
| :--- | :--- | :--- |
| **Spacetime Metric** | [`include/kerr/physics/KerrSpacetime.hpp`](include/kerr/physics/KerrSpacetime.hpp) | Analytic Boyer-Lindquist metric, event horizon $r_\pm$, outer ergosphere |
| **Geodesics** | [`src/physics/Geodesic.cpp`](src/physics/Geodesic.cpp) | First-order equations of motion, Carter constant, null condition |
| **Integrators** | [`include/kerr/numerics/Integrators.hpp`](include/kerr/numerics/Integrators.hpp) | RK4 and adaptive DOP853 numerical steppers |
| **Observers & Tetrads** | [`src/physics/Observer.cpp`](src/physics/Observer.cpp) | Local orthonormal frames, ZAMO, aberration correction |
| **Vulkan Compute** | [`shaders/metric_v1.comp`](shaders/metric_v1.comp) | High-throughput GPU parallel geodesic compute pipeline |
| **WebGL2 Engine** | [`web-demo/src/`](web-demo/src/) | Real-time interactive ray tracer, shaders, and UI |
| **Validation Suite** | [`tests/`](tests/) | Unit tests for Schwarzchild limit, extremal spin, and Carter conservation |

---

## Reference & Documentation

- [Physics & Metric Formulation](PHYSICS.md)
- [Numerical Integration & Stability](NUMERICS.md)
- [Architecture & Interfaces](ARCHITECTURE.md)
- [GPU / Vulkan Compute Specifications](GPU_PHYSICS.md)
- [Movie Exporter Manual](MOVIE-EXPORT.md)
- [Arch Linux Runbook](ARCH-START-HERE.md)
- [Project Roadmap](ROADMAP.md)
