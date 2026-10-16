# Rendering a merger film

Open **KerrLab(Arch)** and click **Render movie** in the bottom action bar.
Choose the merger event, resolution, frame rate and duration, then click
**Choose folder & render**. The default is **12 seconds, 1920×1080, 60 FPS**.
Select **144 FPS** for a separate version for a 144 Hz display. The selector
also offers 24 and 30 FPS; resolutions range from 720p through 4K.

The whole merger is included, regardless of the preview playhead or slow-motion
slider. The inspiral and plunge occupy approximately the first 80% of the clip;
the final 20% shows the remnant. This is cinematic time remapping of the existing
approximate model. It does not represent twelve seconds of physical evolution.
The existing merger has no accretion disk during the binary phase and the export
preserves that model. The video includes the rendered sky and bloom, with no
interface, waveform strip or audio.

To frame a shot, select the event in the Merger panel, orbit/zoom to the desired
view, adjust sky/bloom/ray steps as desired, then open Render movie and choose
the same event. If the event differs, its standard camera framing is used.
The camera stays fixed during export. Camera paths and temporal supersampling
are not implemented by this slice.

Every frame renders at the selected pixel dimensions. The preview resolution
slider and automatic merger detail do not reduce export detail. Ray integration
settings are held fixed. A 12-second 60 FPS clip contains 720 separately rendered
frames; at 144 FPS it contains 1,728. Rendering takes as long as necessary and
does not need to keep up with real-time playback. Increasing resolution or frame
rate increases the work. This does not promise realtime 144 FPS or a render-time
estimate on this GPU.

## Output and recovery

Each run creates a new `KerrLab-GW…-*` folder in the directory you select:

- `merger.mp4`: H.264, CRF 17, yuv420p, created after encoding and verification.
- `frames/frame-000000.png`, etc.: original lossless frames, starting at zero.
- `manifest.json`: settings, camera, model, graphics identity, encoder versions,
  frame count, timeline convention, status and verification results.

The MP4 is checked for dimensions, frame count, duration and frame rate, and
decoded completely before being reported as complete. At 144 FPS, use a video
player/decoder that supports that rate and set the display to 144 Hz. Some
players or sharing services may limit playback to 60 FPS. The 60 FPS export is
available for ordinary playback.

PNG frames are retained on success and on cancellation. The dialog estimates
disk space conservatively using uncompressed pixels plus an allowance; actual
PNG size depends on the scene. Cancellation stops after an in-flight frame or
terminates the encoder, then restores the preview. Close/reload/crash handling
retains completed files. An abrupt OS termination may leave a `.partial` file
and a manifest still marked rendering/encoding. There is no automatic render
resume; a new render uses a fresh folder.

The manifest contains an argument list for recovering an MP4 from completed
frames. For example, from a **completed 144 FPS** frame-sequence folder:

```bash
ffmpeg -n -framerate 144 -start_number 0 -i frames/frame-%06d.png -an -c:v libx264 -preset medium -crf 17 -pix_fmt yuv420p -movflags +faststart recovered.mp4
```

An incomplete sequence gives a shorter clip. The command does not overwrite
an existing movie. PNG-only export works when the encoder is unavailable.

## Dependencies and verification

MP4 export uses the existing external `ffmpeg` and `ffprobe` executables on PATH,
with the `libx264` encoder. This Arch installation has both. No encoder source
or binary is copied into KerrLab. The portable desktop package therefore needs
those executables installed for MP4; it remains usable for PNG sequences without
them. Encoder versions are recorded per export rather than silently bundled.

Run the packaged integration check from a normal desktop terminal:

```bash
node tools/check-desktop.cjs --movie
```

It checks the dialog, preload/IPC, exact-time repeated rendering, restored scene
state, and actual 12-second clips at 60 and 144 FPS at 128×72. Native folder
selection and a full-resolution visual acceptance run remain manual checks.
Unit/integration tests also exercise real video encoding with generated images;
those images are not evidence that a merger rendered correctly on the GPU.

Current implementation/evidence: [movie export record](benchmarks/results/2026-10-21-movie-export.md).
