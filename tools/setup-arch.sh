#!/usr/bin/env bash
# Explicit system setup only. The app launcher never invokes this script.
set -euo pipefail

usage() {
  cat <<'HELP'
Usage: bash tools/setup-arch.sh [--runtime|--dev] [--print]
  --runtime  Install shared libraries required by the packaged app (default).
  --dev      Also install a system development toolchain for desktop/native work.
  --print    Print the pacman command without changing anything.

This performs an Arch full system upgrade plus the selected packages using
pacman -Syu --needed. Pacman keeps its normal confirmation and sudo may require
your password. No GPU kernel driver, desktop environment, or security setting
is selected or changed by this script. Set up your working desktop first.
The bundled app needs neither system Node.js nor system Electron to run.
HELP
}

mode=runtime
print_only=0
for option in "$@"; do
  case "$option" in
    --runtime) mode=runtime ;;
    --dev) mode=dev ;;
    --print) print_only=1 ;;
    --help|-h) usage; exit 0 ;;
    *) printf 'Unknown option: %s (use --help).\n' "$option" >&2; exit 1 ;;
  esac
done
# GTK brings its GLib/Pango/Cairo/X11 dependencies; mesa supplies libgbm.
# Sources: https://archlinux.org/packages/extra/x86_64/electron40/
#          https://www.electronjs.org/docs/latest/development/build-instructions-linux
packages=(alsa-lib gtk3 nss libcups libxss libxtst libdrm libxkbcommon libglvnd mesa xdg-utils unzip xz)
if [[ $mode == dev ]]; then
  packages+=(base-devel cmake ninja nodejs npm python shellcheck git desktop-file-utils)
fi
if ((print_only)); then
  printf 'sudo pacman -Syu --needed'
  printf ' %q' "${packages[@]}"
  printf '\n'
  exit 0
fi
[[ $(uname -s) == Linux && -f /etc/arch-release ]] || { printf 'This setup script requires Arch Linux or an Arch derivative with pacman.\n' >&2; exit 1; }
command -v pacman >/dev/null 2>&1 || { printf 'pacman was not found.\n' >&2; exit 1; }
printf 'Installing KerrLab(Arch) %s prerequisites through a full Arch upgrade.\n' "$mode"
if ((EUID == 0)); then
  pacman -Syu --needed "${packages[@]}"
else
  command -v sudo >/dev/null 2>&1 || { printf 'sudo is unavailable. An administrator can run this setup script; launch the app as your normal user afterwards.\n' >&2; exit 1; }
  sudo pacman -Syu --needed "${packages[@]}"
fi
printf 'Prerequisites installed. Run as your normal desktop user: bash start-kerrlab-arch.sh --check\n'
