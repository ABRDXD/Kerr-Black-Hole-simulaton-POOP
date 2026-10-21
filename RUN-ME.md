# KerrLab on this laptop

Open **KerrLab(Arch)** from the application menu, or run `kerrlab` in a terminal.
From this folder you can also run `./start-kerrlab-arch.sh`.

Use the four scene buttons at the bottom to show different views. Drag to orbit
and scroll to zoom. F11 toggles fullscreen; H hides the controls; Escape restores
the interface. Save PNG opens a preview, then Download PNG opens a Save dialog.

**Render movie** exports a complete merger. Choose 12 seconds and either
**60 FPS** or **144 FPS**, select a resolution, and choose an output folder.
It renders each frame at fixed full detail, then saves an MP4 plus lossless PNGs.
See [MOVIE-EXPORT.md](MOVIE-EXPORT.md) for framing, timing, recovery and playback.

The starting internal resolution is 60% for smoother interaction. Expand Quality
and increase the resolution slider for finer detail, or lower it for more speed.
The separate HQ WebGPU still renderer is unavailable on the current backend.

To rebuild the desktop app, close it and run:

```bash
./start-kerrlab-arch.sh --build --no-open
```

The bundled development tools work offline. For the independent native CPU
tools and tests, run `bash tools/build-native.sh --jobs 4`. Executables are in
`out/build/linux-gcc-release/`; each tool supports `--help`.

For repeatable scene/PNG checks and performance measurements, run from a normal
desktop terminal after rebuilding:

```bash
node tools/check-desktop.cjs --measure --power-state "AC, balanced"
```

Use your actual power source/profile in the label. This opens an isolated test
window and writes results to a fresh `out/desktop-check-*` folder. Keep the window
visible. The native Save dialog still needs a manual check; the test supplies its
own save destination. See `web-demo/e2e/DESKTOP-CHECKS.md` for scope and limitations.

Keep this folder together: the menu entry and terminal command point here.
The original archives and KerrLab-Source-Raw folder are preserved in Projects.
