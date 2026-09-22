# Imported renderer and desktop package

KerrLab's current renderer starts from the complete source tree of
[kyleyhw/black_hole](https://github.com/kyleyhw/black_hole), copied at the user's
explicit request on 2026-10-11. The original README, design documents, shaders,
validation scripts, reference images, and GitHub workflow are retained.

- Pinned revision: `96a6835f2624bc6425682f723a050f397893cb6d`.
- Upstream branch: `claude/kerr-black-hole-raytracer-02tz1w`.
- Archive: `https://codeload.github.com/kyleyhw/black_hole/zip/96a6835f2624bc6425682f723a050f397893cb6d`.
- Archive SHA-256: `52cb785e38b1ac50e63b3bb92f0608b000113d2fca2a4f8c991b0e272f5d851c`.
- Import: all 124 files, 9,860,772 bytes before local changes/dependency installation.
- `UPSTREAM_FILES.json` records every original relative path and its SHA-256.

No LICENSE, COPYING, or NOTICE file was found in the pinned tree; GitHub's
license field was null. Public source availability is not recorded as a
permissive license grant. The user explicitly requested this local import
after the license finding; that request is not a new upstream license.
Original attribution is retained and no replacement license is invented.
No public deployment or redistribution was performed.

Local additions include the KerrLab cinematic scene interface, reliable scene
reset/controls refresh, visible startup errors, WebGL resource checks, monotone
frame timing, PNG preview and Windows launch helpers. Electron 44.7.0 now hosts
the renderer in a standalone window loading packaged files directly. Its shell,
packager, package scripts and dependency pin are local additions. The original
manifest records pre-modification hashes, not hashes of the adapted current tree.
The geodesic and image-formation shaders remain upstream implementations.
The 2026-10-14 continuation adds visible RAF timing/diagnostics (separate from
capture and simulation clocks), Electron 44 console-event handling, clearer
load errors, local timing/shell regression tests, and a packaged desktop check
runner. See `e2e/DESKTOP-CHECKS.md`; its target GPU runs are still pending.
The 2026-10-21 movie slice adds a deterministic merger export clock, full-resolution
frame capture, a movie dialog (including 60/144 FPS), a sandboxed preload with
restricted IPC, a recoverable PNG sequence writer and external FFmpeg encoding.
All new export orchestration is locally authored. The shader/model formulas are
unchanged. See `../MOVIE-EXPORT.md` for usage and evidence limits.
Local evidence and limitations are recorded in
`../benchmarks/results/2026-10-12-desktop.md`; future work is in `../ROADMAP.md`.

This browser implementation is independent of KerrLab's C++20/Vulkan core.
Passing the native CPU/GPU tests does not validate the imported WebGL shaders.
Historical reports under `validation/`, `bench/`, and `e2e/` are upstream
artifacts, not evidence of tests run in this workspace. The copied Linux
Playwright harness needs adaptation before it can run on Windows.

The single-Kerr view numerically integrates light paths; finite precision,
finite ray budgets, disk-emission prescriptions, procedural stars and display
bloom remain approximations or artistic choices. The binary-merger metric is
an approximate superposition, not a numerical-relativity solution. WebGPU HQ
stills use a separate shader and do not reproduce every live compositor effect.

To work on the copy with a normal Node.js installation:

```powershell
npm ci --ignore-scripts
npm run dev -- --host 127.0.0.1
npm run build
```

From KerrLab's root, `start-black-hole.ps1 -Build -NoOpen` builds/packages the
desktop app (close it first). `start-black-hole.cmd` launches the current app
under `out/desktop/KerrLab/`. Keep that portable folder intact. `-Browser`
explicitly selects optional web preview; a server is not needed for desktop use.
The helper explicitly installs the pinned Electron runtime after dependency
restoration because `npm ci --ignore-scripts` does not run its installer.
