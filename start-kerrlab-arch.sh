#!/usr/bin/env bash
# Relocatable launcher for the self-contained Linux x86-64 desktop build.
set -euo pipefail

usage() {
  cat <<'HELP'
KerrLab(Arch) - standalone black-hole observatory

Usage: bash start-kerrlab-arch.sh [options]
  --build       Restore bundled offline development tools if needed, rebuild,
                and launch the desktop app. No network installation is performed.
  --no-open     With --build, finish after creating the desktop package.
  --check       Read-only runtime, display, graphics, and sandbox diagnostics.
  --x11         Request X11 / XWayland explicitly.
  --wayland     Request native Wayland explicitly.
  --discrete    Launch through an already installed prime-run (opt-in NVIDIA).
  --help        Show this help.

No flags: launch the packaged app, without Node.js or network access.
Extract the whole bundle into a writable Linux filesystem before rebuilding.
System libraries: bash tools/setup-arch.sh --runtime
App-menu shortcut: bash tools/install-arch-desktop.sh
HELP
}

fail() { printf 'KerrLab(Arch): %s\n' "$*" >&2; exit 1; }
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
build=0
no_open=0
check=0
discrete=0
backend=''
while (($#)); do
  case "$1" in
    --help|-h) usage; exit 0 ;;
    --build) build=1 ;;
    --no-open) no_open=1 ;;
    --check) check=1 ;;
    --discrete) discrete=1 ;;
    --x11|--wayland)
      selected=${1#--}
      [[ -z $backend || $backend == "$selected" ]] || fail 'Choose either --x11 or --wayland.'
      backend=$selected
      ;;
    *) fail "Unknown option: $1 (use --help)." ;;
  esac
  shift
done
((!no_open || build)) || fail '--no-open requires --build.'
if ((check)); then
  ((!build && !no_open && !discrete)) && [[ -z $backend ]] || fail '--check must be used by itself.'
  exec bash "$root/tools/doctor-arch.sh"
fi
[[ $(uname -s) == Linux ]] || fail 'This launcher requires Linux. On Windows use start-black-hole.cmd.'
[[ $(uname -m) == x86_64 ]] || fail 'This bundle contains Linux x86-64 binaries; other CPU architectures need a separate build.'
((EUID != 0)) || fail 'Run KerrLab as your normal desktop user, never with sudo or as root.'

if ((build)); then
  [[ -f $root/tools/restore-arch-dev.sh ]] || fail 'Offline developer restore is missing. Restore the complete migration bundle.'
  # Restore checks the package-lock hash and skips unchanged dependencies.
  bash "$root/tools/restore-arch-dev.sh"
  if [[ -x $root/.tools/node-linux/bin/node ]]; then
    node_bin="$root/.tools/node-linux/bin/node"
  elif command -v node >/dev/null 2>&1; then
    node_bin=$(command -v node)
  else
    fail 'Node.js is unavailable after restore. See bash tools/doctor-arch.sh.'
  fi
  (
    cd -- "$root/web-demo"
    export PATH="$(dirname -- "$node_bin"):$PATH"
    "$node_bin" node_modules/typescript/bin/tsc --noEmit
    "$node_bin" node_modules/vite/bin/vite.js build
    "$node_bin" --test tests/*.test.ts tests/*.test.cjs
    "$node_bin" ../tools/package-desktop.cjs --platform linux --arch x64
  )
  if ((no_open)); then
    printf 'Built: %s\n' "$root/out/desktop/KerrLab-Arch/kerrlab-arch"
    exit 0
  fi
fi

app="$root/out/desktop/KerrLab-Arch/kerrlab-arch"
[[ -f $app ]] || fail 'Packaged Linux app is missing. Restore the complete bundle, or use --build.'
[[ -x $app ]] || fail 'The app is not executable. Extract the tar archive on a Linux filesystem with permissions preserved; do not launch from a noexec USB mount.'
[[ -n ${DISPLAY:-} || -n ${WAYLAND_DISPLAY:-} ]] || fail 'No graphical session detected. Open a terminal in your Linux desktop and run this launcher there.'
if [[ $backend == wayland && -z ${WAYLAND_DISPLAY:-} ]]; then
  fail '--wayland requires a running Wayland session.'
fi
if [[ $backend == x11 && -z ${DISPLAY:-} ]]; then
  fail '--x11 requires an X11 or XWayland display.'
fi
args=()
[[ -z $backend ]] || args+=("--ozone-platform=$backend")
# Preserve the caller's environment; Electron's Node-only mode must not leak in.
if ((discrete)); then
  command -v prime-run >/dev/null 2>&1 || fail '--discrete needs prime-run and a configured NVIDIA driver. Configure your Arch graphics stack first; no driver is installed by this launcher.'
  exec env -u ELECTRON_RUN_AS_NODE prime-run "$app" "${args[@]}"
fi
exec env -u ELECTRON_RUN_AS_NODE "$app" "${args[@]}"
