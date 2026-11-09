#!/usr/bin/env python3
"""Fetch and verify the pinned CEF binary distribution.

Supply-chain rules:
  * The CEF version is pinned in this file.
  * The SHA-1 recorded below is for the Windows x64 minimal archive and was
    obtained from https://cef-builds.spotifycdn.com/index.json.
  * No archive is ever extracted unless its checksum matches.
  * CEF binaries are not committed to git.

Usage:
    python scripts/fetch_cef.py [--force]
"""

from __future__ import annotations

import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request
from pathlib import Path

CEF_VERSION = "156.0.26+g8ea0c73+chromium-156.0.8078.88"
CEF_CHROMIUM_VERSION = "156.0.8078.88"

# Platform -> (index platform key, archive suffix, known SHA-1 or None)
PLATFORM_ARCHIVES = {
    "windows": ("windows64", "windows64_minimal.tar.bz2",
                "15d29eea209d857c22dc163706080295aea69b5c"),
    "linux": ("linux64", "linux64_minimal.tar.bz2", None),
    "darwin": ("macosarm64", "macosarm64_minimal.tar.bz2", None),
}


def archive_name_for(suffix: str) -> str:
    return f"cef_binary_{CEF_VERSION}_{suffix}"

INDEX_URL = "https://cef-builds.spotifycdn.com/index.json"
BASE_URL = "https://cef-builds.spotifycdn.com/"

REPO_ROOT = Path(__file__).resolve().parent.parent
CEF_DIR = REPO_ROOT / "third_party" / "cef"
DOWNLOADS_DIR = CEF_DIR / "downloads"
EXTRACT_DIR = CEF_DIR / "binary"
STAMP = CEF_DIR / ".installed"


def platform_key() -> str:
    system = platform.system().lower()
    if system == "windows":
        return "windows"
    if system == "linux":
        return "linux"
    if system == "darwin":
        return "darwin"
    raise SystemExit(f"Unsupported platform: {system}")


def fetch_url(url: str, dest: Path) -> None:
    print(f"Downloading {url}")
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".part")
    req = urllib.request.Request(url, headers={"User-Agent": "nexus-build/0.1"})
    with urllib.request.urlopen(req, timeout=60) as response, open(tmp, "wb") as out:
        total = int(response.headers.get("Content-Length", "0"))
        seen = 0
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            out.write(chunk)
            seen += len(chunk)
            if total:
                pct = seen * 100 // total
                print(f"\r  {pct:3d}%  {seen / 1048576:.1f}/{total / 1048576:.1f} MiB", end="")
        print()
    tmp.replace(dest)


def sha1(path: Path) -> str:
    digest = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resolve_checksum(archive_name: str, known: str | None) -> str:
    if known:
        return known
    print("No hard-coded checksum for this platform; resolving from official index.")
    with urllib.request.urlopen(INDEX_URL, timeout=60) as response:
        index = json.load(response)
    key, _, _ = PLATFORM_ARCHIVES[platform_key()]
    for entry in index.get(key, {}).get("versions", []):
        if entry.get("cef_version") == CEF_VERSION:
            for f in entry.get("files", []):
                if f.get("name") == archive_name:
                    return f["sha1"]
    raise SystemExit(f"Could not resolve checksum for {archive_name} in official index.")


def extract(archive: Path) -> None:
    print(f"Extracting {archive.name}")
    if EXTRACT_DIR.exists():
        shutil.rmtree(EXTRACT_DIR)
    EXTRACT_DIR.mkdir(parents=True, exist_ok=True)
    try:
        with tarfile.open(archive, "r:bz2") as tar:
            members = tar.getmembers()
            # Guard against path traversal in upstream archives.
            for member in members:
                target = (EXTRACT_DIR / member.name).resolve()
                if not str(target).startswith(str(EXTRACT_DIR.resolve())):
                    raise SystemExit(f"Unsafe path in archive: {member.name}")
            tar.extractall(EXTRACT_DIR)
    except (tarfile.TarError, EOFError) as exc:
        raise SystemExit(f"Archive extraction failed: {exc}") from exc

    # Upstream archives contain a single top-level cef_binary_* directory.
    # Flatten it so the include/ and Release/ paths are stable.
    children = list(EXTRACT_DIR.iterdir())
    if len(children) == 1 and children[0].is_dir():
        nested = children[0]
        for item in nested.iterdir():
            shutil.move(str(item), str(EXTRACT_DIR / item.name))
        nested.rmdir()


def verify_layout() -> None:
    required = [
        "include/cef_app.h",
        "include/cef_browser.h",
        "libcef_dll/CMakeLists.txt",
    ]
    missing = [p for p in required if not (EXTRACT_DIR / p).exists()]
    if missing:
        raise SystemExit(
            "CEF layout is incomplete after extraction; missing: " + ", ".join(missing)
        )
    lib = EXTRACT_DIR / "Release" / "libcef.dll"
    if not lib.exists():
        raise SystemExit("Release/libcef.dll is missing from the CEF distribution.")


def main() -> int:
    force = "--force" in sys.argv

    if STAMP.exists() and not force:
        stamped = STAMP.read_text(encoding="utf-8").strip()
        if stamped == CEF_VERSION and (EXTRACT_DIR / "include" / "cef_app.h").exists():
            print(f"CEF {CEF_VERSION} already installed at {EXTRACT_DIR}")
            return 0

    key = platform_key()
    _, archive_suffix, known_sha1 = PLATFORM_ARCHIVES[key]
    archive_name = archive_name_for(archive_suffix)
    url = BASE_URL + archive_name
    archive = DOWNLOADS_DIR / archive_name

    expected = resolve_checksum(archive_name, known_sha1)
    if not archive.exists() or sha1(archive) != expected:
        if archive.exists():
            print("Existing archive failed checksum verification; re-downloading.")
            archive.unlink()
        fetch_url(url, archive)

    actual = sha1(archive)
    if actual != expected:
        archive.unlink(missing_ok=True)
        raise SystemExit(
            f"Checksum mismatch for {archive.name}\n  expected {expected}\n  actual   {actual}"
        )
    print(f"Checksum verified: {actual}")

    extract(archive)
    verify_layout()

    STAMP.write_text(CEF_VERSION + "\n", encoding="utf-8")
    (CEF_DIR / "VERSION").write_text(
        f"version={CEF_VERSION}\nchromium={CEF_CHROMIUM_VERSION}\n"
        f"source={INDEX_URL}\nsha1={actual}\n",
        encoding="utf-8",
    )
    print(f"CEF installed: {EXTRACT_DIR}")
    print(f"Version: {CEF_VERSION} (Chromium {CEF_CHROMIUM_VERSION})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
