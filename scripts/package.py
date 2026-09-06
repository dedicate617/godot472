#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MindSCADA Engine Extension - Unified Distribution Packaging Tool.

This script scans compiled Godot binaries in the bin/ directory, organizes them
according to MindStudio and MindSCADA distribution specifications, and produces
standardized archive packages under dist/ along with a SHA256 checksums manifest.

Supported Packages:
1. Windows x86_64 Editor:
   MindStudio_v{version}_Windows_x86_64.zip
   - godot.windows.editor.x86_64.exe (or dev build)
   - godot.windows.editor.x86_64.console.exe (if present)
   - WebView2Loader.dll runtime library
   - Default README.txt documentation

2. Windows x86_64 Export Templates:
   MindSCADA_v{version}_export_templates_windows.tpz (standard Godot templates/ structure)
   - templates/windows_debug_x86_64.exe
   - templates/windows_debug_x86_64_console.exe (and .console.exe)
   - templates/windows_release_x86_64.exe
   - templates/windows_release_x86_64_console.exe (and .console.exe)
   - templates/version.txt

3. Linux ARM32 (Pi32) Export Templates:
   MindSCADA_v{version}_export_templates_pi32.tpz (standard Godot templates/ structure)
   - templates/linux_debug_arm32 & templates/linux_debug.arm32
   - templates/linux_release_arm32 & templates/linux_release.arm32
   - templates/version.txt

4. Linux ARM64 (Pi64) Export Templates:
   MindSCADA_v{version}_export_templates_pi64.tpz (standard Godot templates/ structure)
   - templates/linux_debug_arm64 & templates/linux_debug.arm64
   - templates/linux_release_arm64 & templates/linux_release.arm64
   - templates/version.txt

Checksum Manifest:
   dist/checksums.sha256 (GNU sha256sum format)

Usage:
    python scripts/package.py [--version 4.7.2] [--godot-dir <path>]
                              [--dist-dir dist] [--platform <all|windows|pi32|pi64>]
                              [--dry-run]
"""

import argparse
import hashlib
import os
import shutil
import sys
import time
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

def safe_path(p: Any) -> Path:
    """Normalize path to absolute without resolving mapped network drives to UNC on Windows."""
    return Path(os.path.abspath(str(p)))

# ANSI Color codes for terminal logging
COLOR_RESET = "\033[0m"
COLOR_BOLD = "\033[1m"
COLOR_RED = "\033[91m"
COLOR_GREEN = "\033[92m"
COLOR_YELLOW = "\033[93m"
COLOR_BLUE = "\033[94m"
COLOR_MAGENTA = "\033[95m"
COLOR_CYAN = "\033[96m"


def _colorize(text: str, color_code: str) -> str:
    """Return colored text if stdout is a terminal, else plain text."""
    if sys.stdout.isatty():
        return f"{color_code}{text}{COLOR_RESET}"
    return text


def log_info(msg: str) -> None:
    print(f"{_colorize('[INFO]', COLOR_BLUE)} {msg}")


def log_success(msg: str) -> None:
    print(f"{_colorize('[SUCCESS]', COLOR_GREEN)} {msg}")


def log_warn(msg: str) -> None:
    print(f"{_colorize('[WARNING]', COLOR_YELLOW)} {msg}")


def log_error(msg: str) -> None:
    print(f"{_colorize('[ERROR]', COLOR_RED)} {msg}")


def log_dry_run(msg: str) -> None:
    print(f"{_colorize('[DRY-RUN]', COLOR_MAGENTA)} {msg}")


def log_step(msg: str) -> None:
    print(f"\n{_colorize('===>', COLOR_CYAN)} {_colorize(msg, COLOR_BOLD)}")


# ---------------------------------------------------------------------------
# Path Probing & Environment Discovery
# ---------------------------------------------------------------------------

def probe_repo_root() -> Path:
    """Resolve the root directory of the mindscada-engine-ext repository."""
    return safe_path(Path(__file__).parent.parent)


def probe_godot_dir(repo_root: Path, custom_path: Optional[str] = None) -> Path:
    """
    Probe the Godot engine source directory.
    Priority:
    1. Explicit custom_path if provided.
    2. Sibling directory ../godot.4.7.2
    3. Sibling directory ../godot
    4. Sibling directory ../godot.4.2.1
    5. Parent directory containing SConstruct or bin/
    6. Current working directory containing SConstruct or bin/
    7. Fallback: ../godot.4.7.2
    """
    if custom_path:
        p = safe_path(custom_path)
        if p.name == "bin" and p.is_dir():
            return p.parent
        return p

    candidates = [
        repo_root.parent / "godot.4.7.2",
        repo_root.parent / "godot",
        repo_root.parent / "godot.4.2.1",
    ]

    for cand in candidates:
        if (cand / "bin").is_dir() or (cand / "SConstruct").is_file():
            return safe_path(cand)

    for cand in candidates:
        if cand.is_dir():
            return safe_path(cand)

    if (repo_root.parent / "SConstruct").is_file() or (repo_root.parent / "bin").is_dir():
        return safe_path(repo_root.parent)

    if (Path.cwd() / "SConstruct").is_file() or (Path.cwd() / "bin").is_dir():
        return safe_path(Path.cwd())

    return safe_path(repo_root.parent / "godot.4.7.2")


def resolve_bin_dir(godot_dir: Path) -> Path:
    """Resolve the bin/ directory from a probed or provided Godot path."""
    if godot_dir.name == "bin" and godot_dir.is_dir():
        return godot_dir
    return godot_dir / "bin"


def find_first_existing(directory: Path, candidate_names: List[str]) -> Optional[Path]:
    """Find the first matching file from candidate names in the given directory."""
    if not directory.is_dir():
        return None
    for name in candidate_names:
        target = directory / name
        if target.is_file():
            return target
    return None


def compute_file_sha256(filepath: Path) -> str:
    """Compute standard SHA256 hex digest for a file."""
    hasher = hashlib.sha256()
    with open(filepath, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            hasher.update(chunk)
    return hasher.hexdigest()


# ---------------------------------------------------------------------------
# Packaging Specification Models
# ---------------------------------------------------------------------------

@dataclass
class ArchiveItem:
    """Represents a single entry inside a target archive."""
    arcname: str
    source_file: Optional[Path] = None
    content_bytes: Optional[bytes] = None
    is_executable: bool = False
    description: str = ""


@dataclass
class PackagePlan:
    """Specification of an archive to be generated."""
    name: str
    platform_key: str
    archive_filename: str
    items: List[ArchiveItem] = field(default_factory=list)
    missing_required: List[str] = field(default_factory=list)
    missing_optional: List[str] = field(default_factory=list)

    @property
    def is_viable(self) -> bool:
        """True if the package has at least one essential artifact to package."""
        return len(self.items) > 0 and len(self.missing_required) == 0


# ---------------------------------------------------------------------------
# Artifact Collector Functions
# ---------------------------------------------------------------------------

def generate_default_readme(version: str) -> str:
    """Generate default README text for MindStudio Editor package."""
    return f"""========================================================================
MindStudio v{version}
MindSCADA Engine Extension - Windows x86_64 Editor Distribution
========================================================================

Version    : {version}
Platform   : Windows x86_64
Architecture: x86_64 (64-bit MSVC)

Included Components:
- MindStudio / Godot Engine Editor Executable
- Microsoft Edge WebView2 Loader Runtime (WebView2Loader.dll)
- Console Wrapper Executable (if bundled)

Instructions:
1. Extract the contents of this archive to a directory of your choice.
2. Ensure WebView2Loader.dll remains adjacent to the editor executable.
3. Launch godot.windows.editor.x86_64.exe (or godot.windows.editor.dev.x86_64.exe).

Export Templates:
Export templates for Windows, Linux ARM32 (Pi32), and Linux ARM64 (Pi64)
can be installed via the Editor menu:
  Project -> Export -> Manage Export Templates -> Install from file (.tpz)

Repository: https://github.com/mindscada/mindscada-engine-ext
========================================================================
"""


def collect_windows_editor(
    bin_dir: Path,
    repo_root: Path,
    version: str,
) -> PackagePlan:
    """Collect artifacts for MindStudio Windows x86_64 Editor package."""
    archive_name = f"MindStudio_v{version}_Windows_x86_64.zip"
    plan = PackagePlan(
        name="Windows x86_64 Editor",
        platform_key="windows",
        archive_filename=archive_name,
    )

    # 1. Main Editor executable
    # Check release first, fallback to dev build
    editor_candidates = [
        "godot.windows.editor.x86_64.exe",
        "MindSCADA.windows.editor.x86_64.exe",
        "MindStudio.windows.editor.x86_64.exe",
        "godot.windows.editor.dev.x86_64.exe",
        "MindSCADA.windows.editor.dev.x86_64.exe",
        "godot.windows.tools.64.exe",
    ]
    editor_exe = find_first_existing(bin_dir, editor_candidates)
    if editor_exe:
        arc_exe_name = editor_exe.name
        plan.items.append(
            ArchiveItem(
                arcname=arc_exe_name,
                source_file=editor_exe,
                is_executable=True,
                description=f"Editor Executable ({editor_exe.name})",
            )
        )
        if arc_exe_name != "godot.windows.editor.x86_64.exe" and "dev" not in arc_exe_name:
            plan.items.append(
                ArchiveItem(
                    arcname="godot.windows.editor.x86_64.exe",
                    source_file=editor_exe,
                    is_executable=True,
                    description="Editor Executable Alias (godot.windows.editor.x86_64.exe)",
                )
            )
    else:
        plan.missing_required.append("godot.windows.editor.x86_64.exe (or dev build)")

    # 2. Console executable (optional)
    console_candidates = [
        "godot.windows.editor.x86_64.console.exe",
        "MindSCADA.windows.editor.x86_64.console.exe",
        "godot.windows.editor.dev.x86_64.console.exe",
        "MindSCADA.windows.editor.dev.x86_64.console.exe",
    ]
    console_exe = find_first_existing(bin_dir, console_candidates)
    if console_exe:
        plan.items.append(
            ArchiveItem(
                arcname=console_exe.name,
                source_file=console_exe,
                is_executable=True,
                description=f"Editor Console Wrapper ({console_exe.name})",
            )
        )
    else:
        plan.missing_optional.append("godot.windows.editor.x86_64.console.exe")

    # 3. Runtime Libraries (WebView2Loader.dll)
    webview_candidates = [
        bin_dir / "WebView2Loader.dll",
        repo_root / "modules" / "webview" / "lib" / "native" / "x64" / "WebView2Loader.dll",
        repo_root / "deps" / "windows_x64" / "WebView2Loader.dll",
    ]
    webview_dll: Optional[Path] = None
    for cand in webview_candidates:
        if cand.is_file():
            webview_dll = cand
            break

    if webview_dll:
        plan.items.append(
            ArchiveItem(
                arcname="WebView2Loader.dll",
                source_file=webview_dll,
                is_executable=False,
                description=f"WebView2 Runtime Loader ({webview_dll.relative_to(repo_root) if repo_root in webview_dll.parents else webview_dll.name})",
            )
        )
    else:
        plan.missing_optional.append("WebView2Loader.dll")

    # Include any additional .dlls present in bin/
    if bin_dir.is_dir():
        for extra_dll in sorted(bin_dir.glob("*.dll")):
            if extra_dll.name.lower() != "webview2loader.dll":
                plan.items.append(
                    ArchiveItem(
                        arcname=extra_dll.name,
                        source_file=extra_dll,
                        is_executable=False,
                        description=f"Runtime Library ({extra_dll.name})",
                    )
                )

    # 4. Default documentation
    readme_content = generate_default_readme(version)
    plan.items.append(
        ArchiveItem(
            arcname="README.txt",
            content_bytes=readme_content.encode("utf-8"),
            is_executable=False,
            description="Default MindStudio Documentation (README.txt)",
        )
    )

    return plan


def collect_windows_templates(
    bin_dir: Path,
    version: str,
) -> PackagePlan:
    """Collect artifacts for MindSCADA Windows x86_64 Export Templates package."""
    archive_name = f"MindSCADA_v{version}_export_templates_windows.tpz"
    plan = PackagePlan(
        name="Windows x86_64 Export Templates",
        platform_key="windows",
        archive_filename=archive_name,
    )

    # 1. Debug executable
    debug_candidates = [
        "godot.windows.template_debug.x86_64.exe",
        "MindSCADA.windows.template_debug.x86_64.exe",
        "windows_debug_x86_64.exe",
    ]
    debug_exe = find_first_existing(bin_dir, debug_candidates)
    if debug_exe:
        plan.items.append(
            ArchiveItem(
                arcname="templates/windows_debug_x86_64.exe",
                source_file=debug_exe,
                is_executable=True,
                description=f"Debug Template ({debug_exe.name})",
            )
        )
    else:
        plan.missing_required.append("windows_debug_x86_64.exe (godot.windows.template_debug.x86_64.exe)")

    # 2. Debug console executable
    debug_console_candidates = [
        "godot.windows.template_debug.x86_64.console.exe",
        "MindSCADA.windows.template_debug.x86_64.console.exe",
        "windows_debug_x86_64.console.exe",
        "windows_debug_x86_64_console.exe",
    ]
    debug_con_exe = find_first_existing(bin_dir, debug_console_candidates)
    if debug_con_exe:
        plan.items.append(
            ArchiveItem(
                arcname="templates/windows_debug_x86_64_console.exe",
                source_file=debug_con_exe,
                is_executable=True,
                description=f"Debug Console Template ({debug_con_exe.name})",
            )
        )
        plan.items.append(
            ArchiveItem(
                arcname="templates/windows_debug_x86_64.console.exe",
                source_file=debug_con_exe,
                is_executable=True,
                description=f"Debug Console Template Compatibility Alias ({debug_con_exe.name})",
            )
        )
    else:
        plan.missing_optional.append("windows_debug_x86_64_console.exe")

    # 3. Release executable
    release_candidates = [
        "godot.windows.template_release.x86_64.exe",
        "MindSCADA.windows.template_release.x86_64.exe",
        "windows_release_x86_64.exe",
    ]
    release_exe = find_first_existing(bin_dir, release_candidates)
    if release_exe:
        plan.items.append(
            ArchiveItem(
                arcname="templates/windows_release_x86_64.exe",
                source_file=release_exe,
                is_executable=True,
                description=f"Release Template ({release_exe.name})",
            )
        )
    else:
        plan.missing_required.append("windows_release_x86_64.exe (godot.windows.template_release.x86_64.exe)")

    # 4. Release console executable
    release_console_candidates = [
        "godot.windows.template_release.x86_64.console.exe",
        "MindSCADA.windows.template_release.x86_64.console.exe",
        "windows_release_x86_64.console.exe",
        "windows_release_x86_64_console.exe",
    ]
    release_con_exe = find_first_existing(bin_dir, release_console_candidates)
    if release_con_exe:
        plan.items.append(
            ArchiveItem(
                arcname="templates/windows_release_x86_64_console.exe",
                source_file=release_con_exe,
                is_executable=True,
                description=f"Release Console Template ({release_con_exe.name})",
            )
        )
        plan.items.append(
            ArchiveItem(
                arcname="templates/windows_release_x86_64.console.exe",
                source_file=release_con_exe,
                is_executable=True,
                description=f"Release Console Template Compatibility Alias ({release_con_exe.name})",
            )
        )
    else:
        plan.missing_optional.append("windows_release_x86_64_console.exe")

    # 5. version.txt (Godot Export Template Manager requirement)
    version_txt_content = f"{version}.stable\n"
    plan.items.append(
        ArchiveItem(
            arcname="templates/version.txt",
            content_bytes=version_txt_content.encode("utf-8"),
            is_executable=False,
            description=f"Godot Export Template Version Tag ({version}.stable)",
        )
    )

    return plan


def collect_pi32_templates(
    bin_dir: Path,
    version: str,
) -> PackagePlan:
    """Collect artifacts for MindSCADA Linux ARM32 (Pi32) Export Templates package."""
    archive_name = f"MindSCADA_v{version}_export_templates_pi32.tpz"
    plan = PackagePlan(
        name="Linux ARM32 (Pi32) Export Templates",
        platform_key="pi32",
        archive_filename=archive_name,
    )

    # 1. Debug executable
    debug_candidates = [
        "godot.linuxbsd.template_debug.arm32",
        "MindSCADA.linuxbsd.template_debug.arm32",
        "linux_debug_arm32",
        "linux_debug.arm32",
        "godot.linuxbsd.template_debug.arm32.llvm",
    ]
    debug_bin = find_first_existing(bin_dir, debug_candidates)
    if debug_bin:
        plan.items.append(
            ArchiveItem(
                arcname="templates/linux_debug_arm32",
                source_file=debug_bin,
                is_executable=True,
                description=f"Linux ARM32 Debug Template ({debug_bin.name})",
            )
        )
        plan.items.append(
            ArchiveItem(
                arcname="templates/linux_debug.arm32",
                source_file=debug_bin,
                is_executable=True,
                description=f"Linux ARM32 Debug Template Compatibility Alias ({debug_bin.name})",
            )
        )
    else:
        plan.missing_required.append("linux_debug_arm32 (godot.linuxbsd.template_debug.arm32)")

    # 2. Release executable
    release_candidates = [
        "godot.linuxbsd.template_release.arm32",
        "MindSCADA.linuxbsd.template_release.arm32",
        "linux_release_arm32",
        "linux_release.arm32",
        "godot.linuxbsd.template_release.arm32.llvm",
    ]
    release_bin = find_first_existing(bin_dir, release_candidates)
    if release_bin:
        plan.items.append(
            ArchiveItem(
                arcname="templates/linux_release_arm32",
                source_file=release_bin,
                is_executable=True,
                description=f"Linux ARM32 Release Template ({release_bin.name})",
            )
        )
        plan.items.append(
            ArchiveItem(
                arcname="templates/linux_release.arm32",
                source_file=release_bin,
                is_executable=True,
                description=f"Linux ARM32 Release Template Compatibility Alias ({release_bin.name})",
            )
        )
    else:
        plan.missing_required.append("linux_release_arm32 (godot.linuxbsd.template_release.arm32)")

    # 3. version.txt
    version_txt_content = f"{version}.stable\n"
    plan.items.append(
        ArchiveItem(
            arcname="templates/version.txt",
            content_bytes=version_txt_content.encode("utf-8"),
            is_executable=False,
            description=f"Godot Export Template Version Tag ({version}.stable)",
        )
    )

    return plan


def collect_pi64_templates(
    bin_dir: Path,
    version: str,
) -> PackagePlan:
    """Collect artifacts for MindSCADA Linux ARM64 (Pi64) Export Templates package."""
    archive_name = f"MindSCADA_v{version}_export_templates_pi64.tpz"
    plan = PackagePlan(
        name="Linux ARM64 (Pi64) Export Templates",
        platform_key="pi64",
        archive_filename=archive_name,
    )

    # 1. Debug executable
    debug_candidates = [
        "godot.linuxbsd.template_debug.arm64",
        "MindSCADA.linuxbsd.template_debug.arm64",
        "linux_debug_arm64",
        "linux_debug.arm64",
        "godot.linuxbsd.template_debug.arm64.llvm",
    ]
    debug_bin = find_first_existing(bin_dir, debug_candidates)
    if debug_bin:
        plan.items.append(
            ArchiveItem(
                arcname="templates/linux_debug_arm64",
                source_file=debug_bin,
                is_executable=True,
                description=f"Linux ARM64 Debug Template ({debug_bin.name})",
            )
        )
        plan.items.append(
            ArchiveItem(
                arcname="templates/linux_debug.arm64",
                source_file=debug_bin,
                is_executable=True,
                description=f"Linux ARM64 Debug Template Compatibility Alias ({debug_bin.name})",
            )
        )
    else:
        plan.missing_required.append("linux_debug_arm64 (godot.linuxbsd.template_debug.arm64)")

    # 2. Release executable
    release_candidates = [
        "godot.linuxbsd.template_release.arm64",
        "MindSCADA.linuxbsd.template_release.arm64",
        "linux_release_arm64",
        "linux_release.arm64",
        "godot.linuxbsd.template_release.arm64.llvm",
    ]
    release_bin = find_first_existing(bin_dir, release_candidates)
    if release_bin:
        plan.items.append(
            ArchiveItem(
                arcname="templates/linux_release_arm64",
                source_file=release_bin,
                is_executable=True,
                description=f"Linux ARM64 Release Template ({release_bin.name})",
            )
        )
        plan.items.append(
            ArchiveItem(
                arcname="templates/linux_release.arm64",
                source_file=release_bin,
                is_executable=True,
                description=f"Linux ARM64 Release Template Compatibility Alias ({release_bin.name})",
            )
        )
    else:
        plan.missing_required.append("linux_release_arm64 (godot.linuxbsd.template_release.arm64)")

    # 3. version.txt
    version_txt_content = f"{version}.stable\n"
    plan.items.append(
        ArchiveItem(
            arcname="templates/version.txt",
            content_bytes=version_txt_content.encode("utf-8"),
            is_executable=False,
            description=f"Godot Export Template Version Tag ({version}.stable)",
        )
    )

    return plan


# ---------------------------------------------------------------------------
# Archive Builder & Checksum Manifest Generator
# ---------------------------------------------------------------------------

def write_archive_entry(zf: zipfile.ZipFile, item: ArchiveItem) -> None:
    """Write an ArchiveItem into the active ZipFile, preserving executable permissions."""
    zinfo = zipfile.ZipInfo(filename=item.arcname)
    zinfo.date_time = time.localtime(time.time())[:6]
    zinfo.compress_type = zipfile.ZIP_DEFLATED

    # Set file attributes / UNIX permissions
    if item.is_executable:
        zinfo.create_system = 3  # UNIX system flag
        # 0o100755: regular file with rwxr-xr-x
        zinfo.external_attr = (0o100755) << 16
    else:
        zinfo.create_system = 3
        # 0o100644: regular file with rw-r--r--
        zinfo.external_attr = (0o100644) << 16

    if item.content_bytes is not None:
        zf.writestr(zinfo, item.content_bytes)
    elif item.source_file is not None:
        with open(item.source_file, "rb") as src, zf.open(zinfo, "w") as dst:
            shutil.copyfileobj(src, dst, length=1024 * 1024)


def build_archive(
    plan: PackagePlan,
    dist_dir: Path,
    dry_run: bool = False,
) -> Optional[Path]:
    """
    Build the compressed archive (.zip or .tpz) for a PackagePlan.
    In dry-run mode, logs all simulated operations without creating files.
    """
    out_archive = dist_dir / plan.archive_filename

    log_step(f"Packaging Target: {plan.name}")
    log_info(f"Archive Output: {out_archive}")

    for item in plan.items:
        if item.source_file:
            size_mb = item.source_file.stat().st_size / (1024 * 1024)
            size_str = f"{size_mb:.2f} MB"
            log_info(f"  + [{item.arcname}] <= {item.source_file} ({size_str})")
        else:
            byte_len = len(item.content_bytes or b"")
            log_info(f"  + [{item.arcname}] <= (in-memory data, {byte_len} bytes)")

    if dry_run:
        log_dry_run(f"Would create archive: {out_archive}")
        return out_archive

    dist_dir.mkdir(parents=True, exist_ok=True)
    temp_archive = dist_dir / f"{plan.archive_filename}.tmp"

    try:
        with zipfile.ZipFile(temp_archive, mode="w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            for item in plan.items:
                write_archive_entry(zf, item)

        if out_archive.exists():
            out_archive.unlink()
        temp_archive.rename(out_archive)

        archive_size_mb = out_archive.stat().st_size / (1024 * 1024)
        log_success(f"Generated archive: {out_archive} ({archive_size_mb:.2f} MB)")
        return out_archive
    except Exception as e:
        if temp_archive.exists():
            temp_archive.unlink()
        log_error(f"Failed to generate archive {out_archive}: {e}")
        return None


def generate_checksums(
    dist_dir: Path,
    dry_run: bool = False,
) -> Optional[Path]:
    """
    Scan all .zip and .tpz archives in dist_dir, compute SHA256 hashes,
    and write to checksums.sha256 in GNU standard format.
    """
    checksum_file = dist_dir / "checksums.sha256"
    log_step("Generating SHA256 Checksums Manifest")

    if dry_run:
        log_dry_run(f"Would scan {dist_dir} for (*.zip, *.tpz) and write manifest to {checksum_file}")
        return checksum_file

    if not dist_dir.is_dir():
        log_warn(f"Distribution directory {dist_dir} does not exist. No checksums generated.")
        return None

    # Collect all .zip and .tpz files
    archives: List[Path] = []
    for ext in ("*.zip", "*.tpz"):
        archives.extend(dist_dir.glob(ext))

    archives = sorted(set(archives), key=lambda p: p.name.lower())
    if not archives:
        log_warn(f"No archive files (*.zip, *.tpz) found in {dist_dir}. Checksums file skipped.")
        return None

    manifest_lines: List[str] = []
    for archive in archives:
        sha256_hash = compute_file_sha256(archive)
        line = f"{sha256_hash}  {archive.name}"
        manifest_lines.append(line)
        log_info(f"SHA256: {sha256_hash}  {archive.name}")

    manifest_content = "\n".join(manifest_lines) + "\n"
    checksum_file.write_text(manifest_content, encoding="utf-8")
    log_success(f"SHA256 manifest saved to: {checksum_file} ({len(archives)} archive(s))")

    return checksum_file


# ---------------------------------------------------------------------------
# CLI Argument Parser & Orchestrator
# ---------------------------------------------------------------------------

def create_parser() -> argparse.ArgumentParser:
    """Configure command-line argument parser for packaging driver."""
    parser = argparse.ArgumentParser(
        description="MindSCADA Engine Extension - Unified Distribution Packaging Tool.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Dry-run packaging for all available targets with default version:
  python scripts/package.py --dry-run

  # Package all available targets for MindSCADA 4.7.2:
  python scripts/package.py --version 4.7.2

  # Package Windows targets explicitly specifying godot source directory:
  python scripts/package.py --platform windows --godot-dir ../godot.4.7.2

  # Package Linux ARM32 export templates to custom dist directory:
  python scripts/package.py --platform pi32 --dist-dir ./my_dist
        """,
    )

    parser.add_argument(
        "--version",
        type=str,
        default="4.7.2",
        help="Product distribution version string (default: '4.7.2')",
    )
    parser.add_argument(
        "--godot-dir",
        type=str,
        default=None,
        help="Path to Godot source root or bin/ directory (default: auto-detected)",
    )
    parser.add_argument(
        "--dist-dir",
        type=str,
        default=None,
        help="Target distribution output directory (default: '<repo_root>/dist')",
    )
    parser.add_argument(
        "--platform",
        choices=["all", "windows", "pi32", "pi64"],
        default="all",
        help="Target packaging scope: 'all', 'windows', 'pi32', or 'pi64' (default: 'all')",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=False,
        help="Scan and simulate packaging operations without writing files",
    )

    return parser


def run_packaging(args: argparse.Namespace) -> int:
    """Execute or simulate distribution packaging workflow."""
    repo_root = probe_repo_root()
    godot_dir = probe_godot_dir(repo_root, args.godot_dir)
    bin_dir = resolve_bin_dir(godot_dir)
    dist_dir = safe_path(args.dist_dir) if args.dist_dir else safe_path(repo_root / "dist")
    version = args.version.strip()

    banner = "=" * 75
    print(banner)
    mode_suffix = " (DRY-RUN SIMULATION)" if args.dry_run else ""
    log_info(f"MindSCADA Engine Extension - Distribution Packager{mode_suffix}")
    print(banner)
    log_info(f"Target Version     : {version}")
    log_info(f"Repository Root    : {repo_root}")
    log_info(f"Godot Directory    : {godot_dir}")
    log_info(f"Binary Directory   : {bin_dir} ({'EXISTS' if bin_dir.is_dir() else 'NOT FOUND'})")
    log_info(f"Distribution Dir   : {dist_dir}")
    log_info(f"Platform Filter    : {args.platform}")
    print(banner)

    if not bin_dir.is_dir():
        log_error(f"Godot binary directory does not exist: {bin_dir}")
        log_error("Please compile engine targets first using scripts/build.py or pass --godot-dir.")
        if not args.dry_run:
            return 1

    # Define candidate packages according to platform filter
    selected_platform = args.platform.lower()

    plans: List[PackagePlan] = []
    if selected_platform in ("all", "windows"):
        plans.append(collect_windows_editor(bin_dir, repo_root, version))
        plans.append(collect_windows_templates(bin_dir, version))
    if selected_platform in ("all", "pi32"):
        plans.append(collect_pi32_templates(bin_dir, version))
    if selected_platform in ("all", "pi64"):
        plans.append(collect_pi64_templates(bin_dir, version))

    # Evaluate viability of plans
    viable_plans: List[PackagePlan] = []
    skipped_plans: List[Tuple[PackagePlan, str]] = []

    for plan in plans:
        if plan.missing_required:
            reason = f"Missing required file(s): {', '.join(plan.missing_required)}"
            skipped_plans.append((plan, reason))
        elif len(plan.items) == 0:
            skipped_plans.append((plan, "No matching artifacts found"))
        else:
            viable_plans.append(plan)

    # Report warnings for skipped plans
    if skipped_plans:
        log_step("Platform Scan Warnings & Skipped Targets")
        for plan, reason in skipped_plans:
            if selected_platform == "all":
                log_warn(f"Skipping '{plan.name}': {reason}")
            else:
                log_error(f"Target '{plan.name}' cannot be packaged: {reason}")

    # Check if any viable plans remain
    if not viable_plans:
        log_error("No viable packages could be built from the binaries in bin/.")
        if args.dry_run:
            log_dry_run("Dry-run finished with 0 packages to build.")
            return 0
        return 1

    # Execute builds for viable plans
    log_step(f"Executing Packaging Pipeline ({len(viable_plans)} target package(s))")
    created_archives: List[Path] = []
    for plan in viable_plans:
        out = build_archive(plan, dist_dir, dry_run=args.dry_run)
        if out:
            created_archives.append(out)

    # Generate Checksums Manifest
    manifest = generate_checksums(dist_dir, dry_run=args.dry_run)

    print(f"\n{banner}")
    if args.dry_run:
        log_success(f"Dry-run simulation completed successfully. {len(created_archives)} package(s) planned.")
    else:
        log_success(f"Packaging completed successfully. {len(created_archives)} archive(s) generated in {dist_dir}.")
        if manifest:
            log_success(f"Manifest written to {manifest}")
    print(banner)

    return 0


def main() -> int:
    parser = create_parser()
    args = parser.parse_args()
    return run_packaging(args)


if __name__ == "__main__":
    sys.exit(main())
