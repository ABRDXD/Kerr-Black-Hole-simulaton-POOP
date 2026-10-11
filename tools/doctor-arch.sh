#!/usr/bin/env bash
# Read-only checks. Never install drivers, change sysctls, or disable sandboxing.
set -euo pipefail

if [[ ${1:-} == --help || ${1:-} == -h ]]; then
  printf 'Usage: bash tools/doctor-arch.sh\nChecks the packaged Linux runtime and graphics-session prerequisites without changing system settings.\n'
  exit 0
fi
[[ $# == 0 ]] || { printf 'Unknown option (use --help).\n' >&2; exit 1; }
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
app="$root/out/desktop/KerrLab-Arch/kerrlab-arch"
problem=0
ok() { printf 'OK    %s\n' "$*"; }
note() { printf 'NOTE  %s\n' "$*"; }
bad() { printf 'FIX   %s\n' "$*"; problem=1; }
printf 'KerrLab(Arch) diagnostics\n'
if [[ $(uname -s) != Linux ]]; then
  bad 'This bundle targets Linux; run this check after booting Arch.'
  exit "$problem"
fi
if [[ $(uname -m) == x86_64 ]]; then ok 'CPU: x86-64'; else bad 'CPU: this bundle requires x86-64.'; fi
if ((EUID == 0)); then bad 'Running as root: open your normal user desktop terminal instead.'; else ok 'Running as a normal user'; fi
if [[ -f /etc/arch-release ]]; then ok 'Arch Linux system'; else note 'Non-Arch system: setup-arch.sh is not applicable; shared library requirements still apply.'; fi
if [[ -f $app ]]; then
  ok 'Packaged Electron app is present'
  if [[ -x $app ]]; then ok 'App executable permission'; else bad 'App is not executable; extract the original tar archive on a Linux filesystem with permissions preserved.'; fi
  if command -v findmnt >/dev/null 2>&1; then
    mount_options=$(findmnt -n -o OPTIONS --target "$app" 2>/dev/null || true)
    if [[ ,$mount_options, == *,noexec,* ]]; then bad 'App is on a noexec mount. Move/extract the bundle into your Linux home directory.'; fi
  fi
  if command -v ldd >/dev/null 2>&1; then
    # Only examine our bundled, pinned Electron binary, not arbitrary input.
    ldd_status=0
    libraries=$(ldd "$app" 2>&1) || ldd_status=$?
    if [[ $libraries == *'not found'* ]]; then
      bad 'Missing shared libraries (run bash tools/setup-arch.sh --runtime):'
      while IFS= read -r line; do [[ $line != *'not found'* ]] || printf '      %s\n' "$line"; done <<< "$libraries"
    elif ((ldd_status != 0)); then
      bad 'The bundled executable could not be inspected; verify the bundle checksum and architecture.'
    else
      ok 'No missing libraries reported by ldd'
    fi
  else
    note 'ldd unavailable: shared library inspection was skipped.'
  fi
else
  bad 'Packaged app missing. Restore the complete migration archive, or run bash start-kerrlab-arch.sh --build --no-open.'
fi
if [[ -f $root/out/desktop/KerrLab-Arch/resources/app/dist/index.html ]]; then ok 'Rendered app assets are present'; else bad 'App assets are missing from resources/app/dist.'; fi
if [[ -n ${WAYLAND_DISPLAY:-} ]]; then ok 'Wayland display is advertised'; fi
if [[ -n ${DISPLAY:-} ]]; then ok 'X11 / XWayland display is advertised'; fi
if [[ -z ${DISPLAY:-} && -z ${WAYLAND_DISPLAY:-} ]]; then bad 'No graphical display. Run inside your desktop session, not a bare TTY or SSH shell.'; fi
if [[ -n ${DBUS_SESSION_BUS_ADDRESS:-} ]]; then ok 'Desktop D-Bus session is advertised'; else note 'No desktop D-Bus session detected; use your normal desktop session for dialogs and app integration.'; fi
if [[ -r /proc/sys/kernel/unprivileged_userns_clone && $(</proc/sys/kernel/unprivileged_userns_clone) == 0 ]]; then
  bad 'Unprivileged user namespaces are disabled. Review your distribution sandbox policy; do not use --no-sandbox or run the app as root.'
elif [[ -r /proc/sys/user/max_user_namespaces && $(</proc/sys/user/max_user_namespaces) == 0 ]]; then
  bad 'User namespaces are disabled by max_user_namespaces. Review your distribution sandbox policy.'
else
  note 'No disabled-user-namespace sysctl detected. Actual sandbox startup still needs a successful app launch.'
fi
if command -v prime-run >/dev/null 2>&1; then note 'prime-run is available; --discrete opts into NVIDIA offload.'; else note 'prime-run is not installed; default GPU selection remains with your desktop/driver.'; fi
if command -v lspci >/dev/null 2>&1; then
  while IFS= read -r line; do
    case "$line" in *'VGA compatible controller:'*|*'3D controller:'*|*'Display controller:'*) printf 'GPU   %s\n' "$line" ;; esac
  done < <(lspci 2>/dev/null || true)
fi
if command -v glxinfo >/dev/null 2>&1 && command -v timeout >/dev/null 2>&1 && [[ -n ${DISPLAY:-} ]]; then
  graphics=$(timeout 10 glxinfo -B 2>/dev/null || true)
  while IFS= read -r line; do
    case "$line" in *'OpenGL vendor string:'*|*'OpenGL renderer string:'*|*'Accelerated:'*) printf 'GL    %s\n' "$line" ;; esac
  done <<< "$graphics"
  if [[ -z $graphics ]]; then note 'glxinfo did not return a result; native Wayland may still work.'; fi
else
  note 'Optional glxinfo probe skipped (requires mesa-utils and an X11/XWayland display).'
fi
note 'OpenGL diagnostics are not a WebGL2 test. Launch KerrLab, check its image/FPS, and keep Auto merger detail enabled on this laptop.'
if [[ -x $root/.tools/node-linux/bin/node ]]; then ok 'Bundled development Node is restored'; else note 'Development Node is not restored; --build restores the offline tools when needed. Node is not needed to run the packaged app.'; fi
printf 'Result: '
if ((problem)); then printf 'address the FIX items before launching.\n'; else printf 'basic prerequisites passed; a real desktop render is still required to verify GPU performance.\n'; fi
exit "$problem"

// refactor: polish doctor-arch.sh implementation details
