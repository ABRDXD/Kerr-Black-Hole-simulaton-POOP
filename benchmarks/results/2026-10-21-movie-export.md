# Merger movie export — 2026-10-21

User request: implement a 12-second cinematic merger clip, including normal
60 FPS and a 144 FPS version for a 144 Hz laptop display. This is a requested
D6 slice ahead of the broader scene/session roadmap.

## Delivered

- **Render movie** desktop dialog: event, duration (1–120 whole seconds),
  resolution (720p/1080p/1440p/4K), 24/30/60/144 FPS, MP4 or PNG-only.
  Defaults are 12 seconds, 1080p, 60 FPS; choose 144 FPS for another export.
- Fixed frame-index clock, independent of preview FPS and capture/encoding time.
  Both endpoints of the selected event are sampled. Inspiral/plunge occupy 80%
  of the sampled clip; the remnant occupies 20%. Output timestamp is index/FPS.
- Same live binary shader, procedural sky and bloom at full output resolution.
  Camera and ray budget stay fixed. Preview quality adaptation is disabled during
  export; prior scene/camera/freefall/merger/quality state is restored afterwards.
  No shader formulas or native physics were changed.
- Sequential acknowledged PNG writes in a unique destination folder, bounded
  per-frame memory, conservative free-space check, retained frames and an atomic
  recovery manifest. Cancellation/reload/window-close/renderer failure handling.
- External system FFmpeg/libx264 produces H.264 CRF 17 yuv420p MP4; ffprobe checks
  dimensions, frame count, FPS and duration; FFmpeg fully decodes the result before
  it is renamed from `movie.partial.mp4` to `merger.mp4` and reported as complete.
- Restricted sandboxed preload API. Main-process handlers validate the sender,
  top-level frame, local URL, export options, PNG dimensions and sequence order.
  Renderer cannot choose arbitrary output paths or execute arbitrary commands.
- Foreground/background export scheduling and an application suspension blocker
  while writing/encoding; both released at completion/failure/cancellation.
- `tools/check-desktop.cjs --movie` extends native checks with the dialog,
  preload/IPC path, repeated-frame determinism, animated/nonblank PNGs, scene
  restoration and actual 12-second 60/144 FPS clips at 128×72.

Source: `web-demo/src/movie-timeline.ts`, `movie-ui.ts`, `main.ts`, `showcase.ts`,
`showcase.css`, `web-demo/desktop/movie-export.cjs`, `movie-ipc.cjs`, `preload.cjs`,
`main.cjs`, three new test files, adapted shell fixtures and desktop harness.
See [usage](../../MOVIE-EXPORT.md) and [provenance](../../THIRD_PARTY.md).

## Verification actually executed

- `node --test --test-isolation=none tests/*.test.ts tests/*.test.cjs` from
  `web-demo`: **43 tests passed, 0 failed, 0 skipped**.
- Real FFmpeg/ffprobe tests encoded generated 64×36 animated test images into
  **12.000-second** videos at **60 FPS / 720 frames** and **144 FPS / 1,728 frames**.
  Independently queried every presentation timestamp and checked index/FPS;
  successful video validation includes complete decoding. These are test images,
  not GPU-rendered merger movies. Temporary test workspaces were removed by the
  tests; no user output was deleted.
- Tests cover invalid/unbounded settings, reordered/duplicate/mismatched frames,
  failed/incomplete exports, cancellation during a pending write and during a real
  running encoder process, idempotent cancellation, retained PNG bytes, fresh-job
  isolation, PNG-only operation without FFmpeg, metadata and IPC sender validation.
- A real encoder cancellation test initially exposed a race between failure and
  cancellation manifest writes. Fixed by awaiting the complete operation including
  its failure journal; subsequent tests and full build passed.
- `bash start-kerrlab-arch.sh --build --no-open`: offline restore, TypeScript/Vite,
  all six test files and Linux desktop packaging **passed**.
- Syntax checks passed for export service, IPC, preload and desktop harness.
- Packaged shell/preload/export files match source; CSP and referenced assets
  checked. Package timestamp: **2026-10-21T09:14:22.037Z**, Electron **44.7.0**.
- Actual packaged native check attempted after rebuild:
  `node tools/check-desktop.cjs --movie`. Output directory:
  `out/desktop-check-eE0oYL`. Native Electron stopped **before JavaScript** with
  missing DRM devices and `sandbox_host_linux.cc:41 ... shutdown: Operation not
  permitted`, SIGTRAP. `launcher.json` correctly reports no completed test report.
  The active session still cannot launch the app's graphics sandbox.

## Remaining acceptance and scope

No newly rendered merger movie or visual/native-folder-dialog acceptance is
claimed from this session. `BUILD-INFO.json` remains `runtimeTestedOnTarget: false`.
The encoder and timeline tests establish export timing, file handling and codec
behavior; they do not establish real GPU frame correctness or full-resolution
performance. Windows runtime verification is also pending.

Run `node tools/check-desktop.cjs --movie` from a normal desktop terminal, then
open KerrLab and export a 12-second 1080p film at 60 FPS and another at 144 FPS.
Inspect the images across collision/remnant, confirm native folder selection and
cancellation/restoration, and play the 144 FPS file with an appropriate player and
display setting. The full-resolution GPU workload may take substantial time.

The movie uses the existing approximate binary model and a fixed camera. Camera
paths, motion blur, audio and automatic resume are not delivered. Export recovery
from completed PNGs is documented. System FFmpeg `n9.0.2` is used as an external
runtime, with its version recorded per export; no new third-party source or
encoder binary was copied/bundled. Broader D6 roadmap acceptance stays open.
