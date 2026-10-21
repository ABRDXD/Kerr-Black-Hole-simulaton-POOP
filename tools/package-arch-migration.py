#!/usr/bin/env python3
"""Create an allowlisted, verified personal Linux migration snapshot and tarball.

Run after the desktop production build and Linux packaging. Does not fetch anything.
Output must be new: no existing user backup is removed or overwritten.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tarfile
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parent.parent
VENDORS = [
    "glfw-7b6aead9fb88b3623e3b3725ebb42670cbe4c579",
    "Vulkan-Headers-2cd90f9d20df57eac214c148f3aed885372ddcfe",
    "glslang-46ef757e048e760b46601e6e77ae0cb72c97bd2f",
    "upstream-black-hole",
]
DOWNLOADS = {
    "node-v24.20.0-linux-x64.tar.xz": "2f2c0da162318f0de47665410c7c8c2ed3d36c8f3105de4bbc61176c70a7cbf2",
    "electron-v44.7.0-linux-x64.zip": "3ae7d5bdad61c664486c6ab61361dd084d064c8c08af8d13a687096e18ce555a",
}


def digest(file):
    with open(file, "rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "out/migration/KerrLab(Arch)")
    parser.add_argument("--archive", type=Path, default=ROOT / "out/migration/KerrLab-Arch-migration.tar.gz")
    args = parser.parse_args()
    output, archive = args.output.resolve(), args.archive.resolve()
    if output == ROOT or ROOT.is_relative_to(output) or output.is_relative_to(ROOT / "web-demo"):
        raise SystemExit("Choose an independent output directory, not the source tree or its parent.")
    if archive.is_relative_to(output):
        raise SystemExit("Archive must be outside the snapshot directory.")
    if output.exists() or archive.exists() or Path(str(archive) + ".sha256").exists():
        raise SystemExit("Output already exists. Use new paths to preserve your previous backup.")
    for name, expected in DOWNLOADS.items():
        if digest(ROOT / ".tools/downloads" / name) != expected:
            raise SystemExit(f"Pinned archive checksum mismatch: {name}")
    for needed in ["out/desktop/KerrLab-Arch/kerrlab-arch", "web-demo/dist/index.html",
                   "ARCH-START-HERE.md", "ARCH-HANDOFF.md", ".tools/arch-npm-cache/_cacache"]:
        if not (ROOT / needed).exists():
            raise SystemExit(f"Missing prerequisite: {needed}")
    output.mkdir(parents=True)
    root_files = {".clang-format", ".gitignore", ".gitattributes", "CMakeLists.txt"}
    for item in ROOT.iterdir():
        if item.is_file() and (item.name in root_files or item.suffix in {".md", ".json", ".sh", ".ps1", ".cmd"}):
            shutil.copy2(item, output / item.name)
    exclusions = shutil.ignore_patterns("node_modules", "__pycache__", ".venv", ".pytest_cache", ".git", "*.pyc")
    for directory in ["include", "src", "tests", "shaders", "benchmarks", "third_party", "tools", "web-demo"]:
        shutil.copytree(ROOT / directory, output / directory, ignore=exclusions)
    for vendor in VENDORS:
        shutil.copytree(ROOT / ".tools" / vendor, output / ".tools" / vendor, ignore=exclusions)
    shutil.copytree(ROOT / "out/desktop/KerrLab-Arch", output / "out/desktop/KerrLab-Arch")
    # Cache entries contain only npm's integrity-checked public packages; omit logs/config.
    shutil.copytree(ROOT / ".tools/arch-npm-cache/_cacache", output / ".tools/arch-npm-cache/_cacache",
                    ignore=shutil.ignore_patterns("tmp"))
    (output / ".tools/downloads").mkdir(parents=True, exist_ok=True)
    for name in DOWNLOADS:
        shutil.copy2(ROOT / ".tools/downloads" / name, output / ".tools/downloads" / name)
    # Pin metadata accompanies the complete vendored notices and upstream manifest.
    metadata = {
        "edition": "KerrLab(Arch)", "target": "linux-x64", "createdAt": datetime.now(timezone.utc).isoformat(),
        "sourceHost": os.name, "electron": "44.7.0", "developmentNode": "24.20.0",
        "runtimeVerifiedOnArch": False, "downloadsSha256": DOWNLOADS,
        "excluded": ["Windows build outputs and compilers", "installed node_modules", "private app/account configuration",
                     "unrelated CodeCraft tooling", "temporary caches and logs", "Codex authentication/chat history"],
    }
    (output / "MIGRATION-INFO.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    entries = []
    for item in sorted(output.rglob("*")):
        if item.is_symlink():
            raise SystemExit(f"Unexpected symlink in snapshot: {item}")
        if item.is_file():
            relative = item.relative_to(output).as_posix()
            if "\n" in relative or "\r" in relative or "\\" in relative:
                raise SystemExit(f"Unsupported filename: {relative}")
            entries.append((relative, digest(item), item.stat().st_size))
    manifest = "".join(f"{checksum}  {name}\n" for name, checksum, _ in entries)
    (output / "SHA256SUMS").write_text(manifest, encoding="utf-8", newline="\n")
    executable_names = {"kerrlab-arch", "chrome-sandbox", "chrome_crashpad_handler"}

    def linux_metadata(info):
        info.uid = info.gid = 0
        info.uname = info.gname = ""
        # Explicit modes survive packing on Windows. No setuid helper or security bypass.
        info.mode = 0o755 if info.isdir() or info.name.endswith(".sh") or Path(info.name).name in executable_names else 0o644
        return info

    archive.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive, "w:gz", compresslevel=6, format=tarfile.PAX_FORMAT) as target:
        target.add(output, arcname="KerrLab(Arch)", filter=linux_metadata)
    # Stream every archived file back through SHA-256. Verifies transfer, names and modes.
    expected = {name: checksum for name, checksum, _ in entries}
    expected["SHA256SUMS"] = digest(output / "SHA256SUMS")
    seen = set()
    with tarfile.open(archive, "r:gz") as source:
        for member in source:
            if not member.isfile():
                continue
            name = member.name.removeprefix("KerrLab(Arch)/")
            if name in seen or name not in expected:
                raise SystemExit(f"Unexpected/duplicate archive member: {member.name}")
            seen.add(name)
            checksum = hashlib.file_digest(source.extractfile(member), "sha256").hexdigest()
            if checksum != expected[name]:
                raise SystemExit(f"Archive verification failed: {name}")
            if (name.endswith(".sh") or Path(name).name in executable_names) and member.mode != 0o755:
                raise SystemExit(f"Executable permissions missing: {name}")
    if seen != set(expected):
        raise SystemExit("Archive file list is incomplete.")
    archive_hash = digest(archive)
    Path(str(archive) + ".sha256").write_text(f"{archive_hash}  {archive.name}\n", encoding="utf-8", newline="\n")
    print(json.dumps({"snapshot": str(output), "archive": str(archive), "filesVerified": len(seen),
                      "archiveBytes": archive.stat().st_size, "sha256": archive_hash}, indent=2))


if __name__ == "__main__":
    main()
