# Local packaged desktop checks

`tools/check-desktop.cjs` is KerrLab's local Electron harness. It loads the actual
packaged main process and assets using the pinned runtime, a fresh profile, and
the normal renderer sandbox. It does not depend on upstream Playwright paths,
a web server, a remote debugging port, or an installed browser.

From a normal desktop terminal in the project root, close the normal app before
rebuilding and run:

```bash
./start-kerrlab-arch.sh --build --no-open
node tools/check-desktop.cjs --measure --power-state "AC, balanced"
```

Replace the power-state description with the actual power source/profile. Omit
`--measure` for just the interaction checks. `--x11` and `--wayland` select a
display backend. The bundled `.tools/node-linux/bin/node` can replace `node`.
The Windows source workspace can use `start-black-hole.ps1 -Build -NoOpen`
and the same Node command (without Linux display flags).

The test window opens separately from your normal app/profile. Keep it visible
and avoid interacting with it while checks run. Each invocation creates a new
`out/desktop-check-*` folder containing logs, report JSON, and image captures;
no prior run is overwritten. `launcher.json` also records early native crashes
that happen before JavaScript can start. Success requires a completed report,
not just a zero process exit code.

## Coverage

`node tools/check-desktop.cjs --movie` additionally checks the movie dialog and
actual preload/IPC export path. It renders 12-second 60/144 FPS clips at 128×72,
checks repeated frame bytes, nonblank/animated images, restored scene state,
and video timing/decoding. It requires system FFmpeg/libx264/ffprobe. The deadline
becomes 30 minutes. Folder selection is redirected to the evidence directory;
the native folder dialog and full-resolution visual acceptance remain manual.
Generated-image encoder tests do not establish GPU rendering correctness.

- Sandboxed packaged file loading, actual WebGL renderer identity, and absence
  of an identified software renderer.
- All four showcase presets, their spins, encoded PNG dimensions and nonuniform
  pixels. PNG files are retained for visual inspection.
- Preview, saved DownloadItem decoded from disk, repeated preview and cancelled
  download. Test code supplies the destination, bypassing the native dialog.
- Reset from active merger/free fall, overlays and nondefault resolution.
- Cinema H/Escape handlers via DOM events; these do not prove physical keyboard
  focus or OS accelerators work.
- Resize canvas consistency with the actual viewport and DPR; the report records
  whether the compositor honored the requested size.

With `--measure`, four fixed configurations run: single Kerr and paused GW151226
at its initial time with automatic detail, manual detail, and retarded automatic
detail. Auto-orbit is off. Each scenario warms up for 3 seconds, clears its timing
history, then collects at least 10 seconds of visible RAF intervals. Screenshots
and pixel readbacks occur before this phase. The report records raw intervals,
sample count, actual duration, mean FPS, p50/p95/p99/max milliseconds, configuration,
camera, output/internal dimensions, changing automatic sizes, GPU/driver information
reported by Electron/WebGL, and end-of-sample process metrics. Controller history
is cleared at sample start, while the current automatic pixel budget is retained.

RAF intervals include CPU work, GPU backpressure, compositor scheduling and vsync.
They are not GPU execution timings. FPS is frames divided by measured elapsed time;
percentiles use nearest rank. Real foreground stalls are retained without the
simulation time cap. A maximum of 3600 recent intervals is retained. Profile changes,
backgrounding and capture/readback clear history. Adaptive resolution is allowed to
change during automatic-detail measurements and its sizes are recorded.

## Remaining manual acceptance

Check the real native Save dialog (save, cancel, focus and repeated use), visually
inspect the four PNGs, exercise the sidebar presets and physical H/Escape/F11,
minimize/restore, scaling/display changes, relaunch, and single-instance focus.
Playing-merger transitions, optional HQ/WebGPU, context loss/recovery, and the
independent native Vulkan window require their own acceptance checks. This harness
does not mark those gates complete.

In a restricted environment, a native sandbox or missing GPU-device failure is
an unavailable runtime check, not evidence of a renderer defect or a successful
benchmark. Run on the normal desktop without disabling the sandbox.
