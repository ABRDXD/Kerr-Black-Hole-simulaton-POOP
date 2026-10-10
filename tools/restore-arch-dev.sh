#!/usr/bin/env bash
# Restore the pinned development environment from the personal migration bundle.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
if [[ ${1:-} == --help ]]; then
  printf '%s\n' 'Usage: bash tools/restore-arch-dev.sh' 'Restores bundled Linux Node, npm dependencies and Electron offline. No sudo.'
  exit 0
fi
[[ $# == 0 ]] || { printf '%s\n' 'Unexpected argument.' >&2; exit 2; }
[[ $(uname -s) == Linux && $(uname -m) == x86_64 ]] || { printf '%s\n' 'Requires Linux x86_64.' >&2; exit 1; }
[[ $EUID != 0 ]] || { printf '%s\n' 'Run as your normal user, not root.' >&2; exit 1; }
for command in sha256sum tar xz unzip; do
  command -v "$command" >/dev/null || { printf 'Missing %s. Run bash tools/setup-arch.sh --runtime\n' "$command" >&2; exit 1; }
done
verify_archive() {
  local file=$1 expected=$2 actual
  [[ -f "$file" ]] || { printf 'Missing bundled archive: %s\n' "$file" >&2; exit 1; }
  actual=$(sha256sum -- "$file")
  [[ ${actual%% *} == "$expected" ]] || { printf 'Checksum mismatch: %s\n' "$file" >&2; exit 1; }
}
NODE_ARCHIVE="$ROOT/.tools/downloads/node-v24.20.0-linux-x64.tar.xz"
ELECTRON_ARCHIVE="$ROOT/.tools/downloads/electron-v44.7.0-linux-x64.zip"
NODE_ROOT="$ROOT/.tools/node-linux"
ELECTRON_ROOT="$ROOT/.tools/electron-linux"
mkdir -p -- "$ROOT/.tools"
if [[ ! -x "$NODE_ROOT/bin/node" ]]; then
  verify_archive "$NODE_ARCHIVE" 2f2c0da162318f0de47665410c7c8c2ed3d36c8f3105de4bbc61176c70a7cbf2
  [[ ! -e "$NODE_ROOT" ]] || { printf 'Incomplete Node folder: %s. Rename it before retrying.\n' "$NODE_ROOT" >&2; exit 1; }
  temporary=$(mktemp -d "$ROOT/.tools/.node-restore.XXXXXX")
  tar -xJf "$NODE_ARCHIVE" --strip-components=1 -C "$temporary" --no-same-owner
  mv -- "$temporary" "$NODE_ROOT"
fi
export PATH="$NODE_ROOT/bin:$PATH"
[[ $(node --version) == v24.20.0 ]] || { printf '%s\n' 'Bundled Node version mismatch.' >&2; exit 1; }
if [[ ! -x "$ELECTRON_ROOT/electron" ]]; then
  verify_archive "$ELECTRON_ARCHIVE" 3ae7d5bdad61c664486c6ab61361dd084d064c8c08af8d13a687096e18ce555a
  [[ ! -e "$ELECTRON_ROOT" ]] || { printf 'Incomplete Electron folder: %s. Rename it before retrying.\n' "$ELECTRON_ROOT" >&2; exit 1; }
  temporary=$(mktemp -d "$ROOT/.tools/.electron-restore.XXXXXX")
  unzip -q "$ELECTRON_ARCHIVE" -d "$temporary"
  chmod 755 "$temporary/electron" "$temporary/chrome-sandbox" "$temporary/chrome_crashpad_handler"
  mv -- "$temporary" "$ELECTRON_ROOT"
fi
[[ $(cat "$ELECTRON_ROOT/version") == 44.7.0 ]] || { printf '%s\n' 'Bundled Electron version mismatch.' >&2; exit 1; }
cd -- "$ROOT/web-demo"
lock_hash=$(sha256sum package-lock.json)
lock_hash=${lock_hash%% *}
marker=node_modules/.kerrlab-lock.sha256
if [[ ! -f node_modules/typescript/bin/tsc || ! -f node_modules/vite/bin/vite.js ||
      ! -f "$marker" || $(cat "$marker") != "$lock_hash" ]]; then
  # Fresh cache is scoped to this project; install hooks are deliberately skipped.
  node "$NODE_ROOT/lib/node_modules/npm/bin/npm-cli.js" ci --offline --ignore-scripts \
    --cache "$ROOT/.tools/arch-npm-cache" --include=dev --no-audit --no-fund
  printf '%s\n' "$lock_hash" > "$marker"
fi
printf '%s\n' 'Pinned Linux development environment restored. No network access was required.'
