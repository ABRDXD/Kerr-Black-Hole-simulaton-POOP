# Third-Party Sources and Provenance

## Local Electron desktop runtime (2026-10-12)

- Dependency: Electron **44.7.0**, exact version in `web-demo/package.json` and
  the local adapted `package-lock.json`; source/package is the official Electron
  npm distribution. The runtime installer was explicitly invoked after restoring
  packages with install scripts disabled.
- License actually inspected: `web-demo/node_modules/electron/LICENSE`, MIT,
  copyright Electron contributors and GitHub. SHA-256:
  `5154e165bd6c2cc0cfbcd8916498c7abab0497923bafcd5cb07673fe8480087d`.
- A retained copy is `third_party/licenses/electron-44.7.0.txt`. The local portable
  assembly preserves Electron's runtime distribution and its bundled Chromium
  notices; this does not claim a separate audit of every transitive Chromium license.
- Use: standalone window around the imported renderer. Local authored shell:
  `web-demo/desktop/main.cjs`; packaging: `tools/package-desktop.cjs`. No Electron
  sample implementation is represented as imported project source.
- This package is a local development deliverable, not a public release. The
  renderer's absent upstream license remains a separate provenance issue below.

## Movie export (2026-10-21)

The movie timeline, dialog, Electron preload/IPC and frame writer were authored
locally. No new third-party implementation source was copied for this feature.
MP4 encoding invokes the user's external FFmpeg and ffprobe executables through
argument arrays, without a shell. Their source/binaries are not bundled or
redistributed. The local installation reports FFmpeg `n9.0.2`, configured with
GPL components including `libx264`; this is not represented as MIT-licensed code.
The independent-process CLI integration adds no FFmpeg source to KerrLab.
Every export records the actual encoder/probe version; PNG-only export works
without these tools. Revisit licensing and exact binary provenance before any
future bundling; this implementation performs no bundling or public release.

## Browser renderer imported at the user's request (2026-10-11)

The user explicitly requested copying the full `kyleyhw/black_hole` project
for the local browser demo, superseding the earlier reference-only workflow
decision for this source. The entire 124-file tree at revision
`96a6835f2624bc6425682f723a050f397893cb6d` is now under `web-demo/`, with original
attribution and documentation retained. No upstream license file was found;
the GitHub license field is null. This is an accurately recorded local import
under the user's instruction, not a claim of a permissive license grant.
No public deployment or redistribution was performed.

The archive SHA-256 is
`52cb785e38b1ac50e63b3bb92f0608b000113d2fca2a4f8c991b0e272f5d851c`.
`web-demo/UPSTREAM_FILES.json` lists every original file/hash;
`web-demo/UPSTREAM.md` records the source URL, exact pin, local modifications,
license status and validation boundary. Local changes add cinematic controls,
startup diagnostics and launch helpers; renderer math/shaders are retained.
The imported browser renderer is separate from the independently implemented
C++/Vulkan core. Native conformance results do not transfer to this renderer.

The initial browser build used the upstream `package-lock.json`. The current
local lock additionally pins Electron 44.7.0; the original lock's hash remains
in `web-demo/UPSTREAM_FILES.json`. Dependencies are restored using
`npm ci --ignore-scripts`. The npm 10.9.4 development CLI is downloaded only
from the official npm registry, with archive SHA-512 checked against registry
metadata, and stays under ignored `.tools/`. Node.js uses the available local
runtime; no global package manager or system PATH changes are made.

## Window continuation dependencies (2026-10-07)

These pins supersede the header/SDK pins in the historical headless entry below.
glslang remains 15.0.0. `tools/bootstrap-vulkan.ps1 -Window -Validation` reproduces
them. Archives and unmodified upstream sources remain in ignored `.tools/`.

| Dependency | Immutable revision | Use and inspected license |
|---|---|---|
| [GLFW 3.4](https://github.com/glfw/glfw/tree/7b6aead9fb88b3623e3b3725ebb42670cbe4c579) | `7b6aead9fb88b3623e3b3725ebb42670cbe4c579` | Unmodified static library through upstream CMake; window, input and Vulkan surface. Actual LICENSE.md is zlib-style; exact notice retained as `glfw-3.4.txt`. |
| [Vulkan-Headers v1.4.321](https://github.com/KhronosGroup/Vulkan-Headers/tree/2cd90f9d20df57eac214c148f3aed885372ddcfe) | `2cd90f9d20df57eac214c148f3aed885372ddcfe` | Unmodified API declarations, including ratified KHR swapchain maintenance. Included core header is Apache-2.0; root LICENSE.md and Apache license inspected. Runtime baseline remains API 1.3 plus checked extensions. |
| [Vulkan-ValidationLayers SDK 1.4.321.0](https://github.com/KhronosGroup/Vulkan-ValidationLayers/tree/fee7b2b4a926355e48668e29570be129eb82cb36) | `fee7b2b4a926355e48668e29570be129eb82cb36` | Development DLL/manifest from Windows SDK 1.4.321.1. Root LICENSE.txt Apache-2.0 with more-permissive exceptions retained verbatim. |
| [SPIRV-Tools SDK 1.4.321.0](https://github.com/KhronosGroup/SPIRV-Tools/tree/33e02568181e3312f49a3cf33df470bf96ef293a) | `33e02568181e3312f49a3cf33df470bf96ef293a` | Development spirv-val/spirv-dis from the same SDK; root LICENSE Apache-2.0 retained. |

Archive SHA-256 pins:

- GLFW: `699bf0b3d0bd422c0212263f30c4b6fc1ab4f67320824b27854f1e5c6949a2a0`.
- Vulkan-Headers: `004547a27896a80eef9d9a172e0a99f528fcee4301c899097143a1a14e9a77fd`.
- LunarG Windows SDK 1.4.321.1 archive: `baaa4f7ca11ed3d82aa1c102b21208915485bbaa473068c763daa425cca468bd`.

Retained notice SHA-256 hashes:

- `glfw-3.4.txt`: `149704059b5d0bf551637e50042dd4de9c2cae921021f6636298911e3a5f9462`.
- `vulkan-headers-1.4.321-apache.txt`: `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`.
- `vulkan-validationlayers-sdk-1.4.321.0.txt`: `956a282096cfc658a59db5d7b1cf03625decfcc32a1059e9ad90a99e897436f7`.
- `spirv-tools-sdk-1.4.321.0.txt`: `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`.

The SDK is only extracted, never installed or executed. The .1 Windows bundle
uses components tagged vulkan-sdk-1.4.321.0; no .1 source tags are claimed. These
hashes are not signature-verification claims. Future binary redistribution must
include GLFW's notice. The wrapper, diagnostic shader and swapchain code are
original KerrLab implementations. Consulted [GLFW Vulkan guide](https://www.glfw.org/docs/3.4/vulkan_guide.html)
and [Khronos semaphore guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html);
no sample implementation was copied.

## Adopted Vulkan development dependencies (2026-10-07)

All downloads/extracted tools stay under ignored `.tools/`. No upstream renderer,
geometry, reflection, or dispatch implementation was copied. KerrLab's wrappers,
limited ABI inspector and shader are independently written from the API and the
project's formula specification. Exact notices are retained in `third_party/licenses/`.

| Dependency | Immutable revision | Files/use | Inspected license |
|---|---|---|---|
| [Vulkan-Headers v1.3.296](https://github.com/KhronosGroup/Vulkan-Headers/tree/29f979ee5aa58b7b005f805ea8df7a855c39ff37) | `29f979ee5aa58b7b005f805ea8df7a855c39ff37` | Unmodified `include/vulkan/vulkan_core.h`, `vk_platform.h`, and transitively included `vk_video` declarations from the local archive | The included generated core header at this revision is **Apache-2.0**, not a presumed MIT alternative. Root LICENSE.md and LICENSES/Apache-2.0.txt inspected. |
| [glslang 15.0.0](https://github.com/KhronosGroup/glslang/tree/46ef757e048e760b46601e6e77ae0cb72c97bd2f) | `46ef757e048e760b46601e6e77ae0cb72c97bd2f` | Unmodified local standalone GLSL compiler; no runtime linkage. HLSL, optimizer, external projects, tests and remapper disabled. | Full root LICENSE.txt retained: BSD-2/3, MIT, Apache-2.0 and NVIDIA preprocessor terms; includes historical Bison GPL text with its exception, and states Bison was removed. No parser/compiler source is incorporated into KerrLab. |
| [Vulkan-ValidationLayers SDK 1.3.296.0](https://github.com/KhronosGroup/Vulkan-ValidationLayers/tree/9935cdd6f9524b1da1e140393fa69a0a1b72125d) | `9935cdd6f9524b1da1e140393fa69a0a1b72125d` | Optional unmodified Windows DLL and JSON manifest extracted from LunarG's SDK; development use only | Root LICENSE.txt: Apache-2.0 with more-permissive file exceptions. |
| [SPIRV-Tools SDK 1.3.296.0](https://github.com/KhronosGroup/SPIRV-Tools/tree/6dcc7e350a0b9871a825414d42329e44b0eb8109) | `6dcc7e350a0b9871a825414d42329e44b0eb8109` | Optional local `spirv-val.exe` / `spirv-dis.exe` from the same SDK; development use only | Root LICENSE: Apache-2.0. |

Archive SHA-256 pins, enforced by `tools/bootstrap-vulkan.ps1`:

- Vulkan-Headers commit archive: `7bc8879373538d3326dabe2df32cb89f94cea5b5d3988134031aea14be000454`.
- glslang commit archive: `913ba37e638ef573023e12e79e194e91beb4297b51016fdad6e4ba0b19cb62e0`.
- LunarG `VulkanSDK-1.3.296.0-Installer.exe`: `acb4ae0786fd3e558f8b3c36cc3eba91638984217ba8a6795ec64d2f9ffd8c4b`.

The SDK installer is read as an archive, never executed. Optional validation tools
are selected with explicit archive include paths. Runtime discovery uses a
process-local VK_LAYER_PATH; no registry or system PATH changes are made.
These hashes pin observed archives; they are not signature-verification claims.

Notice file hashes:

- `vulkan-headers-1.3.296-apache.txt`: `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`.
- `glslang-15.0.0.txt`: `adb783e734e906d1f46db5df29991dbde84bdb0ceab502ac2febb44fe3c2b5f4`.
- `vulkan-validationlayers-sdk-1.3.296.0.txt`: `ac7f9f4384d1cec4c318493e0614a68871a74a4ae6c1f592022b0b8b86690c71`.
- `spirv-tools-sdk-1.3.296.0.txt`: `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`.

The Vulkan loader is an **external driver runtime**, dynamically loaded from the
Windows system directory (or `libvulkan.so.1` on Linux). No loader/volk source or
loader binary is vendored or redistributed. Actual runtime/device versions are
recorded with measurements. Distributable packaging and its full runtime notices
remain future work; local tool installations are not a redistribution bundle.

API references: [Khronos synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html)
for shader-to-host memory visibility and [LunarG SDK guide](https://vulkan.lunarg.com/doc/view/1.3.296.0/windows/getting_started.html)
for development-layer contents. Documentation examples were consulted, not copied.

Audit dates: 2026-09-23 and 2026-09-24. Repository license status can change; re-check and pin a revision
before copying any file. The initial physics foundation was independently implemented. The dense
coefficient table added in the CPU solver is recorded below; no external geodesic or solver control
implementation has been copied.

## Adopted numerical coefficient table

- Source: [SciPy 1.14.1, `scipy/integrate/_ivp/rk.py`, RK45.P](https://github.com/scipy/scipy/blob/92d2a8592782ee19a1161d0bf3fc2241ba78bb63/scipy/integrate/_ivp/rk.py).
- Immutable revision: `92d2a8592782ee19a1161d0bf3fc2241ba78bb63` (peeled v1.14.1 tag).
- License inspected: root `LICENSE.txt`, BSD-3-Clause.
- Exact downloaded license SHA-256: `c29fb8e5936f33237a8b15256b4c6e93037f05bbf58c3d7b5755aede4c200173`.
- Attribution/notice: `third_party/licenses/scipy-1.14.1.txt` and the header comment in `include/kerr/numerics/Integrators.hpp`.
- Adaptation: mathematical quartic dense-output coefficients expressed as a C++ constant table with explicit floating-point literals. No SciPy control flow, class implementation, or Python dependency is incorporated.
- Compatibility: permissive BSD conditions retained; binary distributions must include the notice.
- The main DOPRI5 tableau is independently expressed from the published mathematical table in [Numipedia](https://ketch.github.io/numipedia/methods/DP5.html). Modified-midpoint extrapolation is independently implemented from its mathematical recurrence; no Numerical Recipes or XMDS implementation code is copied.

## Local development tools

Downloaded archives are ignored under `.tools/`; they are not vendored source dependencies.

| Archive | SHA-256 |
|---|---|
| llvm-mingw-20260922-ucrt-x86_64.zip | `e3ad77d117a4bea19a7a3b333341824d79a5a371004a10e25b8504e7b3047666` |
| cmake-4.4.3-windows-x86_64.zip | `4d52ebab7193a698651639ed80d8d04fd903358843572cf44c7fd234cb7c26ab` |
| ninja-win-1.13.2.zip | `07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65` |

These hashes record the local archives; they are not a claim of signature verification. Local builds
copy LLVM-MinGW runtime DLLs alongside executables. A future distributable package must include the
applicable LLVM/MinGW runtime notices from the toolchain, plus the SciPy table notice above.

## Candidate Kerr implementations inspected

| Source | Observed license status | Useful ideas / findings | Reuse decision |
|---|---|---|---|
| [kyleyhw/black_hole](https://github.com/kyleyhw/black_hole) | No license file visible at audited branch or pinned import | Cartesian Kerr-Schild Hamiltonian WebGL/WebGPU design and validation concepts | Earlier reference-only decision superseded by the explicit local import request; see 2026-10-11 entry above. No license grant inferred. |
| [KleaSCM/tika](https://github.com/KleaSCM/tika) | README says MIT, but no `LICENSE` file is present | C++/OpenGL shader; inspected shader uses Boyer-Lindquist components, fixed RK4, axis clamps, and several presentation approximations | License unclear and numerical design unsuitable; do not copy |
| [SethSamuelCode/GRRT](https://github.com/SethSamuelCode/GRRT) | No project license file visible | C++23 CPU/CUDA organization, spectral output concepts | Reference only; its global fast-math flags are specifically unsuitable for a truth path |
| [DamoyY/KerrTrace](https://github.com/DamoyY/KerrTrace) | Apache-2.0 license file present | Cartesian Kerr-Schild, analytic GPU derivatives, Dormand-Prince 5(4), dense disk-event location, spectral LUT architecture | Compatible candidate after file/revision audit; no code copied yet |
| [kerrpy/kerrpy](https://github.com/kerrpy/kerrpy) | GPL-3.0 | Analytical Kerr geodesic reference concepts | Reference only; no incorporation into a differently licensed project |
| [Dyuman-Bhattacharya/kerr-black-hole-raytracer](https://github.com/Dyuman-Bhattacharya/kerr-black-hole-raytracer) | GitHub identifies MIT license | Ingoing coordinates, timelike camera, tetrad and null-ray workflow | Compatible candidate after revision audit; no code copied yet |
| [iAART/aart](https://github.com/iAART/aart) | MIT | Adaptive analytical photon-ring calculations | Candidate validation oracle, not the primary interactive integrator |
| [BlackHolePerturbationToolkit/KerrGeodesics](https://github.com/BlackHolePerturbationToolkit/KerrGeodesics) | MIT | Bound timelike Kerr orbit quantities | Candidate reference vectors for particle validation |

Absence of a license is treated as absence of a reuse grant even when a repository is public. A README claim is not treated as a substitute when the referenced license file is missing.

## Scientific references inspected

| Source | Role |
|---|---|
| [GRay2 paper](https://arxiv.org/abs/1706.07062) | Cartesian Kerr-Schild motivation, null/timelike convergence cases, GPU-oriented formulation |
| [OSIRIS paper](https://doi.org/10.1140/epjc/s10052-022-10054-0) | Hamiltonian integration, multiple solvers, constraint monitoring and validation approach |
| Bardeen, Press, Teukolsky (1972) | ISCO and circular Kerr orbit landmarks |
| Carter (1968) | Separability and Carter invariant |
| Page and Thorne (1974); Novikov and Thorne (1973) | Relativistic thin-disk baseline |

Papers are cited as scientific sources; text and source code are not copied from them.

## Planned infrastructure dependencies

Milestone 4 observer, timelike, and transport code was independently implemented from the written
equations; no additional upstream implementation or coefficient table was copied. Formula checks
used [Bardeen, Press and Teukolsky (1972), Eq. 2.16](https://articles.adsabs.harvard.edu/pdf/1972ApJ...178..347B),
[Schmidt (2002)](https://arxiv.org/abs/gr-qc/0202090), and the parallel-transport discussion in
[Faye and Seraj (2025), section 2](https://arxiv.org/html/2409.02624v2). These are scientific
references, not new runtime dependencies or grants to copy paper text.

The adaptive observer/session continuation adds no runtime dependency or upstream implementation
code. Stationary Fermi-Walker transport and its independent equatorial precession check use
[Kocherlakota et al. (2019), equations (2) and (38)](https://academic.oup.com/mnras/article/490/3/3262/5568376).
At the equator their positive theta direction is KerrLab's negative Cartesian z direction;
the tests preserve this sign and use proper time for the precession rate. These are formula
references only. The connection contraction and adaptive drivers are independently implemented.

This is the historical candidate list. GLFW, Vulkan-Headers, glslang and local
validation tools have now been adopted under the exact pins/licenses above;
their audited entries supersede the expected licenses in this table. Other
components remain candidates requiring revision and license review.

| Component | Expected license | Intended use |
|---|---|---|
| Vulkan-Headers / Vulkan-Hpp | Apache-2.0 OR MIT / Apache-2.0 | Vulkan API declarations |
| Vulkan Loader / volk | Apache-2.0 / MIT | runtime dispatch |
| Vulkan Memory Allocator | MIT | GPU memory allocation |
| SDL3 or GLFW | zlib/libpng or zlib | windows, input, Vulkan surface |
| Dear ImGui | MIT | immediate-mode control UI |
| ImPlot | MIT | scientific plots |
| shaderc/glslang | Apache-2.0 / BSD-3-Clause | shader compilation |
| SPIRV-Reflect | Apache-2.0 | layout conformance |
| Tracy | BSD-3-Clause | optional CPU/GPU profiling |
| toml++ | MIT | versioned human-readable configuration |
| Catch2 | BSL-1.0 | tests, if the dependency improves over the minimal bootstrap harness |

The Khronos Vulkan header repository explicitly marks generated headers `Apache-2.0 OR MIT`: <https://github.com/KhronosGroup/Vulkan-Headers>.

## NVIDIA DLSS

DLSS/NGX uses NVIDIA SDK license terms rather than an ordinary permissive open-source license. The audited programming guide states that Vulkan 1.1+, NGX initialization with a project ID, motion vectors, depth, exposure, and jitter are required. Integration remains optional and deferred. SDK source/binaries will not be committed or redistributed until the current license, download terms, runtime redistribution list, and application-ID process are reviewed at the chosen SDK version.

Sources: [DLSS programming guide](https://github.com/NVIDIA/DLSS/blob/main/doc/DLSS_Programming_Guide_Release.pdf), [NVIDIA RTX SDK license example](https://github.com/NVIDIA-RTX/Streamline/blob/main/external/ngx-sdk/license.txt).

## Provenance procedure for future reuse

Before copying code, add an entry containing repository URL, immutable revision, license file path and hash, copied file/function list, modifications, attribution location, and compatibility conclusion. Preserve required license and NOTICE text in `third_party/licenses/`. If any item is unclear, implement independently from published mathematics or seek permission.
