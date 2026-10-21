#!/usr/bin/env bash
# User-requested full-access Codex session; only applies to this invocation.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
if [[ ${1:-} == --help ]]; then
  printf '%s\n' 'Usage: bash continue-kerrlab-arch.sh [prompt]' \
    'Starts Codex in this project with --ask-for-approval never --sandbox danger-full-access.' \
    'No global settings are changed. Install and sign into Codex first; see ARCH-START-HERE.md.'
  exit 0
fi
[[ $EUID != 0 ]] || { printf '%s\n' 'Start Codex as your normal user, not root.' >&2; exit 1; }
if [[ -x "$ROOT/.tools/node-linux/bin/node" ]]; then export PATH="$ROOT/.tools/node-linux/bin:$PATH"; fi
command -v codex >/dev/null || { printf '%s\n' 'Codex is not installed. Follow ARCH-START-HERE.md, then retry.' >&2; exit 1; }
prompt='Continue KerrLab(Arch). Read AGENTS.md, ARCH-HANDOFF.md and ROADMAP.md. First verify the Arch desktop and measure merger performance on this laptop, then continue the next unfinished milestone. I authorize autonomous local development, dependency setup, fixes, builds and testing within this project; do not repeatedly ask for permission for routine work. Preserve the scientific model boundaries and record actual verification.'
if [[ $# -gt 0 ]]; then prompt="$*"; fi
exec codex --cd "$ROOT" --ask-for-approval never --sandbox danger-full-access "$prompt"
