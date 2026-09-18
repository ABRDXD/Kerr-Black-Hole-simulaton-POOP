# Rendering Design

## Status

The native Vulkan image-formation architecture below remains planned beyond
the existing geometry pipeline and diagnostic window. The current usable
KerrLab desktop app instead renders through the imported WebGL 2 implementation
inside Electron, with a separate optional WebGPU still path. Its basic visible
image is verified; it does not establish completion or accuracy of this native
design. See `ROADMAP.md` for desktop phases, engine integration and acceptance
gates, and `benchmarks/results/2026-10-12-desktop.md` for evidence boundaries.

## Image formation

For each pixel/sample, the compute path will:

1. create a past-directed null direction in the active observer's orthonormal tetrad;
2. transform it to coordinate contravariant momentum and lower the index;
3. integrate the Hamiltonian ray in Cartesian Kerr-Schild coordinates;
4. locate horizon, escape, and radiating-medium events robustly;
5. transport spectral or band-limited invariant intensity using the emitter and observer four-velocities;
6. return radiance plus typed scientific diagnostics.

The primary ray tracer is a Vulkan compute workload. Hardware BVH ray tracing may later accelerate Euclidean subproblems such as spacecraft geometry, but it does not replace curved geodesic integration.

## Proposed pass graph

```text
observer/state upload
        |
ray initialization
        |
batched geodesic integration <---- optional active-ray compaction
        |
event/radiative evaluation
        |
spectral-to-display conversion
        |
temporal accumulation / reconstruction (visual modes only)
        |
tone map + overlays + UI
        |
present
```

Scientific reference captures bypass temporal reconstruction, stochastic disk decoration, bloom, and adaptive numerical changes unless each layer is explicitly requested.

## Radiance and color

The physical core transports frequency-dependent intensity or documented band integrals. Display conversion uses a stated observer colorimetry and exposure. Blackbody lookup tables will be generated and validated against direct Planck integration. RGB shortcuts, if provided for interactive quality levels, are labeled approximations and compared to the spectral reference.

## Environment

An escaped ray samples an environment by its asymptotic direction and frequency shift. Procedural star generation may use a fresh visual seed, while validation uses a fixed catalog/seed. Point-star filtering must account for the severe magnification/sampling problem near caustics; naive single-sample star textures can blink or disappear and will not be described as a physical prediction.

## Disk presentation categories

- **Physically derived:** geodesics, disk four-velocity, invariant redshift factor, baseline flux/temperature law, absorption/emission integration.
- **Physically motivated:** bounded turbulence-inspired emissivity modulation and finite-thickness extensions.
- **Artistic:** bloom, lens dirt, exposure choreography, nonphysical color maps, and exaggerated overlays.

The active category is visible in scientific/hybrid modes and stored in capture metadata.

## Temporal reconstruction and DLSS

DLSS Super Resolution is optional and not a physics feature. NVIDIA's Vulkan integration requires color, depth, accurate motion vectors, exposure, jitter, capability queries, and an application/project ID. Curved-spacetime pixels do not map trivially to Euclidean motion vectors, so the integration must define motion for environment, disk, and local geometry and validate ghosting around the shadow and photon-ring features.

The native path is mandatory. A Vulkan-native temporal/spatial fallback will exist for non-NVIDIA hardware and scientific comparison. DLSS SDK binaries and code are not a hard dependency and will not be redistributed until the applicable NVIDIA license and packaging obligations are reviewed.

Official guide audited: <https://github.com/NVIDIA/DLSS/blob/main/doc/DLSS_Programming_Guide_Release.pdf>.

## Debug outputs

The current `kerr_window` executable is a presentation diagnostic. It dispatches
a deterministic 16x16-workgroup RGBA8 gradient/checkerboard shader, with bounds
checks for partial workgroups and a one-pixel white border. It performs an exact
extent-preserving nearest blit into an RGBA8/BGRA8 UNORM FIFO swapchain with sRGB
nonlinear surface color space. These display codes are not physical radiance,
tone mapping, HDR or black-hole image formation.

One frame is in flight. Acquiring waits at transfer stage, including the first
swapchain layout transition. Render completion and KHR presentation fences are
waited before resource reuse and swapchain replacement. A zero framebuffer or
iconified window suspends dispatch; out-of-date/suboptimal results request
recreation. Missing surface/format/features fail explicitly. Work is bounded to
8192 per axis, 8,388,608 pixels and a requested swapchain count of at most eight.
Fence waits have 30-second deadlines and poll native events between short waits;
the smoke lifecycle has a 45-second deadline. Driver calls themselves may block.

`--capture` reads the compute image synchronously, checks all four channels and
writes binary RGB PPM. Interactive frames do not read back. Presentation fences
permit safe object release and do not guarantee that a frame was displayed;
see the [Khronos fence contract](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html).
Full scientific image validation remains future work. The current Windows
NVIDIA/AMD coverage is recorded in `benchmarks/results/2026-10-07-window.md`.

Render targets or reduced buffers will expose termination class, accepted/rejected steps, minimum step, Hamiltonian residual, invariant drift, redshift factor, emitter class, precision path, and convergence deltas. Error maps use explicit scales and legends.
