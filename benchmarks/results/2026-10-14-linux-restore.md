# Local Arch restore — 2026-10-14

Restored into `/home/abrsdxd/Projects/KerrLab-Restored`. Both migration archives
were byte-identical; the archive checksum and extracted SHA256SUMS passed before
local edits. Original archives and raw source were preserved.

Verified on this Arch x86-64 Wayland desktop:

- Runtime diagnostics: no missing shared libraries; ordinary user sandbox launch
  succeeded without disabling sandboxing. Electron reported hardware WebGL and
  GPU compositing enabled. A screenshot confirmed the visible black-hole image.
- Offline development restore succeeded. TypeScript, Vite production build,
  all 15 quality tests, and Linux desktop packaging succeeded.
- GCC 16.2.1 Release build completed with the project's warning flags; all nine
  CPU CTest suites passed (1.91 seconds). `kerr_trace --help` also ran.
- Installed the user application-menu shortcut and `~/.local/bin/kerrlab` wrapper.
- Reduced normal startup resolution from 1.0 to 0.6; test baseline remains 0.75.
  At approximately 953×1028 window size, two screenshot HUD observations showed
  about 9 fps before and 39 fps after. These are brief observations, not a
  controlled benchmark or a minimum FPS guarantee. The Quality slider remains
  available for manual changes. No physics or integration budgets changed.

Open checks: merger performance, scene-by-scene interaction, native PNG save and
cancel, X11, optional Vulkan diagnostic. The HQ still button reports WebGPU
unavailable despite Electron's general GPU feature status; it was not verified.

Next step: exercise the showcase scenes and PNG save/cancel on this desktop;
measure merger performance before further optimization. Launch instructions are
in `RUN-ME.md`. Original SHA256SUMS describes the archived snapshot, so edited
files and rebuilt outputs no longer match it.
