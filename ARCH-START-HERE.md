# KerrLab(Arch) — restore and run

Personal Arch Linux **x86-64** migration package, prepared on 2026-10-13.
This is the standalone Electron/WebGL black-hole app, complete editable source,
native C++ reference engine, tests, roadmap and offline desktop build dependencies.
The Windows source and launchers are retained. This is not an Arch installer or a
backup of your whole laptop.

**Before formatting: copy the `.tar.gz` and matching `.sha256` file to an external
drive or storage that will survive the wipe.** Files left on this laptop's erased
disk will be lost. The archive has been checked locally; an external backup has
not been made by the agent. Keep the archive until the restored app and source
have been verified on Arch.

## First launch after installing Arch

Install and log into your graphical desktop with a working graphics driver first.
Use your normal user account. Put both migration files in the same directory, then:

```bash
# Run in the directory containing the transferred files.
sha256sum -c 'KerrLab(Arch)-2026-10-13.tar.gz.sha256'
mkdir -p "$HOME/Projects"
tar -xzf 'KerrLab(Arch)-2026-10-13.tar.gz' -C "$HOME/Projects"
cd "$HOME/Projects/KerrLab(Arch)"
sha256sum --quiet -c SHA256SUMS

# Install Arch system libraries once. This needs internet and sudo.
bash tools/setup-arch.sh --runtime
bash start-kerrlab-arch.sh --check
bash start-kerrlab-arch.sh
```

The normal launcher opens **KerrLab(Arch)** in its own window using bundled files.
No browser tab, local web server, system Electron, Node installation, or network
connection is needed for ordinary launches once system libraries are installed.
Keep the entire project folder together. Extract into your Linux home filesystem;
running directly from a USB drive mounted `noexec` will fail. The tar archive
preserves executable permissions, including Electron's helper programs.

Add the app to your desktop's application menu:

```bash
bash tools/install-arch-desktop.sh
```

Re-run the installer after moving the folder. `--remove` removes the menu entry
and icon only. F11 toggles fullscreen; H hides/shows the controls. PNG capture
opens a preview, then a native Save dialog. Final save/cancel behavior on Linux
is an open acceptance check.

## Graphics and the slow merger

The included merger resolution controller targets about 30 fps by reducing
internal image dimensions. It does **not** guarantee 30 fps or change the ray
step budget/physics. Keep **Quality → auto merger detail** enabled; the HUD
shows its current render dimensions. Single-hole scenes retain manual resolution.
The pre-fix Windows merger showed 2 fps; post-fix FPS and Arch FPS have not yet
been measured. [PERFORMANCE.md](PERFORMANCE.md) records the precise boundary.

The launcher lets the desktop choose the display backend and GPU. If needed:

```bash
bash start-kerrlab-arch.sh --wayland
bash start-kerrlab-arch.sh --x11
# Requires a correctly installed NVIDIA driver and prime-run already on the system.
bash start-kerrlab-arch.sh --discrete
```

These are alternatives, not steps to run together. The laptop previously exposed
a Radeon 740M and an RTX 3050 6 GB Laptop GPU on Windows; device numbers and Linux
drivers must be checked again. Setup installs normal shared libraries, including
Mesa's GBM library, but does not select NVIDIA/kernel drivers. Follow the Arch
installation's graphics setup for the actual kernel and hardware.

`bash tools/doctor-arch.sh` reports missing shared libraries, graphics-session
availability, executable permissions, `noexec` mounts and sandbox prerequisites.
Its OpenGL probe does not prove WebGL correctness or performance. Desktop logs
are normally under `${XDG_CONFIG_HOME:-$HOME/.config}/KerrLab(Arch)/desktop.log`.
Do not run the app as root. The renderer sandbox remains enabled.

## Rebuild the desktop app offline

Close KerrLab before rebuilding:

```bash
bash start-kerrlab-arch.sh --build --no-open
bash start-kerrlab-arch.sh
```

The helper restores Linux Node **24.20.0**, npm **11.19.0**, Electron **44.7.0**
and the locked Linux desktop dependencies from the bundled archives/npm cache.
It checks pinned archive hashes, installs npm dependencies with scripts disabled,
runs TypeScript, builds Vite, runs the quality tests, and packages the desktop app.
This workflow needs `tar`, `xz`, `unzip` and the system libraries from setup, but
does not download project dependencies. Initial restore takes more disk space.
If an interrupted restore leaves an incomplete tool folder, the helper identifies
it and asks that it be renamed before retrying; it does not delete an unknown folder.

Changing `package-lock.json` may require fetching new packages. The offline cache
only covers the supplied lockfile. For a deliberate dependency update, use a
normal online npm install, validate the change, and regenerate the migration bundle.

The full Linux restore has **not** been executed on this Windows host. Offline
npm reconstruction was checked with Linux package selection on Windows; that is
not the same as executing the resulting Linux build tools.

## Resume with Codex

Use [ARCH-HANDOFF.md](ARCH-HANDOFF.md) as the new session's starting context.
It preserves your decisions and current unfinished work, so the previous chat
does not need to survive the laptop wipe. Account credentials and Codex chat
history are not included. Install Codex for Linux and sign into your account using
the [official CLI instructions](https://developers.openai.com/codex/cli/).

Once `codex` is available in your terminal:

```bash
cd "$HOME/Projects/KerrLab(Arch)"
bash continue-kerrlab-arch.sh
```

The launcher sets `--ask-for-approval never --sandbox danger-full-access` for this
invocation and supplies the continuation prompt. This is the user's explicitly
requested full-access mode; it permits filesystem and network access beyond the
project and does not modify global Codex settings. The recorded work scope remains
KerrLab. Login, OS-level sudo authentication, and any enforced managed policy are
separate and cannot be removed by project instructions. These settings follow
the [official sandbox/approval documentation](https://learn.chatgpt.com/docs/sandboxing)
and [CLI flags](https://developers.openai.com/codex/cli/reference/).

In an IDE or other Codex client, open this folder and send:

> Continue KerrLab(Arch). Read AGENTS.md, ARCH-HANDOFF.md and ROADMAP.md. Verify
> the Arch desktop first, measure and improve the slow merger, then continue
> the roadmap. I authorize autonomous local project development, dependency
> setup, fixes, builds and testing without repeated permission requests.

That client's own permission settings still apply. The shell helper is the
prepared path for a session with no routine approval prompts.

## Native reference engine

The complete C++ source, headers, tests and pinned vendor sources are included.
The native Vulkan track remains separate from the working Electron image.

```bash
# Install development tools from Arch (internet/sudo needed).
bash tools/setup-arch.sh --dev
bash tools/build-native.sh
bash tools/build-native.sh --configuration Debug
bash tools/build-native.sh --configuration Sanitize
```

CPU-only builds use no external numerical libraries. For optional native Vulkan
checks, install `glslang vulkan-icd-loader vulkan-validation-layers` and the correct
Vulkan driver for your hardware; for the GLFW window target also install
`wayland libxkbcommon xorgproto libx11 libxrandr libxinerama libxcursor libxi`.
Then `bash tools/build-native.sh --vulkan --require-validation` or `--window
--require-validation` selects that track. The window target is a diagnostic image,
not the black-hole renderer. Linux native compilation and CTest are pending.

## What is packed and what is still unverified

The snapshot includes the renderer, built Linux desktop runtime, both tracks'
source/tests, historical reports, full roadmap/handoff, immutable renderer import,
GLFW/Vulkan-Headers/glslang sources, licenses, pinned Linux Node/Electron archives,
and npm cache. `MIGRATION-INFO.json` records versions/exclusions; `SHA256SUMS`
checks every payload file. The archive itself has a separate checksum.

Windows binaries/compilers, installed Windows `node_modules`, unrelated CodeCraft
tools, private account settings and authentication are excluded. Arch itself,
system packages/drivers, a C++ compiler, Codex and account sign-in are external
requirements. This is a personal migration backup, not a public release.

Cross-packaging, production build, static shell checks and archive verification
were performed here. Real Linux startup, Wayland/X11, sandbox behavior, native
save dialogs, actual FPS and Linux native physics tests must be verified after
booting Arch. See [the migration evidence](benchmarks/results/2026-10-13-arch-migration.md).
