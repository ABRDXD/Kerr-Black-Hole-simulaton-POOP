#!/usr/bin/env bash
# Register only this user's app-menu entry and icon; no root access required.
set -euo pipefail

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
  printf 'Usage: bash tools/install-arch-desktop.sh [--remove]\nInstalls the KerrLab(Arch) app-menu entry for your current user. Re-run after moving the project folder.\n'
  exit 0
fi
[[ $# == 0 || ($# == 1 && $1 == --remove) ]] || { printf 'Unknown option (use --help).\n' >&2; exit 1; }
((EUID != 0)) || { printf 'Run this installer as your normal desktop user, without sudo.\n' >&2; exit 1; }
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
data_home=${XDG_DATA_HOME:-$HOME/.local/share}
[[ $data_home == /* ]] || { printf 'XDG_DATA_HOME must be an absolute path.\n' >&2; exit 1; }
desktop="$data_home/applications/kerrlab-arch.desktop"
icon="$data_home/icons/hicolor/scalable/apps/kerrlab-arch.svg"
if [[ ${1:-} == --remove ]]; then
  rm -f -- "$desktop" "$icon"
  printf 'Removed the KerrLab(Arch) app-menu entry and icon. Project files are preserved.\n'
  exit 0
fi
launcher="$root/start-kerrlab-arch.sh"
[[ -f $launcher ]] || { printf 'Project launcher is missing. Keep the complete project folder together.\n' >&2; exit 1; }
[[ -f $root/tools/kerrlab-arch.svg ]] || { printf 'Project icon is missing.\n' >&2; exit 1; }
# Newlines cannot be represented safely across all desktop implementations.
[[ $launcher != *$'\n'* && $launcher != *$'\r'* && $launcher != *$'\t'* ]] || { printf 'Move the project to a path without line breaks or tabs before installing a shortcut.\n' >&2; exit 1; }

# Desktop Exec parsing is not shell parsing. Escape the argument first, then
# its enclosing string value. A literal percent is %% (no field expansion).
# https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html
exec_arg=$launcher
exec_arg=${exec_arg//\\/\\\\}
exec_arg=${exec_arg//\"/\\\"}
exec_arg=${exec_arg//\`/\\\`}
exec_arg=${exec_arg//\$/\\\$}
exec_arg=${exec_arg//%/%%}
exec_arg=${exec_arg//\\/\\\\}
mkdir -p -- "$(dirname -- "$desktop")" "$(dirname -- "$icon")"
install -m 644 -- "$root/tools/kerrlab-arch.svg" "$icon"
{
  printf '[Desktop Entry]\nType=Application\nVersion=1.0\n'
  printf 'Name=KerrLab(Arch)\nComment=Kerr black-hole observatory\n'
  printf 'Exec=/usr/bin/bash "%s"\n' "$exec_arg"
  printf 'Icon=kerrlab-arch\nTerminal=false\nCategories=Education;Science;Astronomy;\n'
  printf 'StartupWMClass=kerrlab-arch\nStartupNotify=true\n'
} > "$desktop"
chmod 644 -- "$desktop"
if command -v desktop-file-validate >/dev/null 2>&1; then
  desktop-file-validate "$desktop"
fi
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$(dirname -- "$desktop")"
fi
printf 'Installed: %s\nThe app menu now contains KerrLab(Arch). Re-run this installer if you move the project folder.\n' "$desktop"
