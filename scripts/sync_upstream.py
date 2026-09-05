#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MindSCADA Engine Extension - Godot Upstream Synchronizer & Patch Application Tool.

This script automates:
1. Cloning or fetching clean Godot engine upstream at a specified tag/branch.
2. Probing and applying atomic patches (patches/<version>/*.patch) in sequential order.
3. Deploying OEM visual assets (patches/assets/) to replace upstream branding.

Usage:
    python scripts/sync_upstream.py [--version 4.7.2-stable] [--target-dir <path>]
                                    [--dry-run] [--skip-clone] [--skip-patch]
                                    [--skip-assets] [--repo-url <url>]
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Optional, Tuple, Dict, Any

# ANSI Color codes for formatted console output
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

def log_deploy(msg: str) -> None:
    print(f"{_colorize('[DEPLOY]', COLOR_CYAN)} {msg}")

def log_skip(msg: str) -> None:
    print(f"{_colorize('[SKIP]', COLOR_YELLOW)} {msg}")


def parse_git_apply_errors(stderr: str) -> List[Dict[str, str]]:
    """
    Parse git apply error messages to extract conflict files and line numbers.

    Common git apply stderr patterns:
      error: patch failed: core/version.h:35
      error: core/version.h: patch does not apply
      error: corrupt patch at line 42
    """
    conflicts: List[Dict[str, str]] = []
    seen: set = set()

    for line in stderr.splitlines():
        line = line.strip()
        # Pattern 1: error: patch failed: <file>:<line>
        m = re.search(r"error:\s+patch failed:\s+([^:]+):(\d+)", line)
        if m:
            entry = {"file": m.group(1).strip(), "line": m.group(2).strip(), "reason": "patch hunk failed"}
            key = (entry["file"], entry["line"])
            if key not in seen:
                seen.add(key)
                conflicts.append(entry)
            continue

        # Pattern 2: error: <file>: patch does not apply
        m2 = re.search(r"error:\s+([^:]+):\s+patch does not apply", line)
        if m2:
            entry = {"file": m2.group(1).strip(), "line": "N/A", "reason": "patch does not apply"}
            key = (entry["file"], entry["line"])
            if key not in seen:
                seen.add(key)
                conflicts.append(entry)
            continue

        # Pattern 3: error: corrupt patch at line <line>
        m3 = re.search(r"error:\s+corrupt patch at line\s+(\d+)", line)
        if m3:
            entry = {"file": "patch_file", "line": m3.group(1).strip(), "reason": "corrupt patch syntax"}
            key = (entry["file"], entry["line"])
            if key not in seen:
                seen.add(key)
                conflicts.append(entry)
            continue

    return conflicts


class UpstreamSyncer:
    """
    Coordinates Godot upstream synchronization, atomic patch application,
    and OEM visual asset replacement.
    """

    def __init__(
        self,
        repo_root: Path,
        version: str = "4.7.2-stable",
        target_dir: Optional[Path] = None,
        dry_run: bool = False,
        skip_clone: bool = False,
        skip_patch: bool = False,
        skip_assets: bool = False,
        repo_url: str = "https://github.com/godotengine/godot.git",
        verbose: bool = False,
    ):
        self.repo_root = repo_root.resolve()
        self.version = version.strip()
        self.dry_run = dry_run
        self.skip_clone = skip_clone
        self.skip_patch = skip_patch
        self.skip_assets = skip_assets
        self.repo_url = repo_url.strip()
        self.verbose = verbose

        # Target directory resolution
        if target_dir is not None:
            self.target_dir = Path(target_dir).resolve()
        else:
            version_clean = self.version.split("-")[0].lstrip("v")
            self.target_dir = (self.repo_root.parent / f"godot.{version_clean}").resolve()

        # Patches and Assets directories resolution
        self.patches_dir = self._resolve_patches_dir()
        self.assets_dir = (self.repo_root / "patches" / "assets").resolve()

        # Execution statistics
        self.stats = {
            "clone_action": "none",
            "patches_checked": 0,
            "patches_applied": 0,
            "patches_skipped": 0,
            "assets_deployed": 0,
        }

    def _resolve_patches_dir(self) -> Path:
        """Find the patches directory corresponding to the target version."""
        patches_base = self.repo_root / "patches"
        candidates = [
            patches_base / self.version,
            patches_base / self.version.split("-")[0],
            patches_base / self.version.lstrip("v"),
            patches_base / self.version.lstrip("v").split("-")[0],
        ]
        for c in candidates:
            if c.is_dir():
                return c.resolve()
        # Default fallback even if doesn't exist yet
        return (patches_base / self.version.split("-")[0]).resolve()

    def run_command(
        self,
        cmd: List[str],
        cwd: Optional[Path] = None,
        check: bool = False,
    ) -> subprocess.CompletedProcess:
        """Execute a system command and return CompletedProcess."""
        work_dir = cwd or self.repo_root
        if self.verbose:
            log_info(f"Running command: {' '.join(cmd)} (cwd={work_dir})")
        return subprocess.run(
            cmd,
            cwd=str(work_dir),
            capture_output=True,
            text=True,
            errors="replace",
            check=check,
        )

    def sync_clone(self) -> bool:
        """Handle Git clone or fetch & checkout."""
        if self.skip_clone:
            log_info(f"Skipping upstream clone/fetch (--skip-clone). Using '{self.target_dir}'.")
            self.stats["clone_action"] = "skipped"
            return True

        if not self.target_dir.exists():
            if self.dry_run:
                log_dry_run(
                    f"Target directory '{self.target_dir}' does not exist. "
                    f"Would clone from '{self.repo_url}' (branch/tag: '{self.version}')."
                )
                self.stats["clone_action"] = "dry-run clone"
                return True

            log_info(f"Target directory '{self.target_dir}' does not exist.")
            log_info(f"Cloning upstream Godot {self.version} from '{self.repo_url}'...")
            self.target_dir.parent.mkdir(parents=True, exist_ok=True)
            clone_cmd = [
                "git", "clone", "--depth", "1",
                "--branch", self.version,
                self.repo_url, str(self.target_dir)
            ]
            res = self.run_command(clone_cmd)
            if res.returncode != 0:
                log_error(f"Failed to clone upstream repository:\n{res.stderr.strip()}")
                return False
            log_success(f"Cloned upstream Godot into '{self.target_dir}'.")
            self.stats["clone_action"] = "cloned"
            return True

        # Target directory exists
        git_dir = self.target_dir / ".git"
        if not git_dir.exists():
            # Check if directory is empty
            if not any(self.target_dir.iterdir()):
                if self.dry_run:
                    log_dry_run(f"Target directory '{self.target_dir}' is empty. Would clone into it.")
                    self.stats["clone_action"] = "dry-run clone"
                    return True
                log_info(f"Target directory '{self.target_dir}' is empty. Cloning...")
                clone_cmd = [
                    "git", "clone", "--depth", "1",
                    "--branch", self.version,
                    self.repo_url, "."
                ]
                res = self.run_command(clone_cmd, cwd=self.target_dir)
                if res.returncode != 0:
                    log_error(f"Failed to clone into '{self.target_dir}':\n{res.stderr.strip()}")
                    return False
                log_success(f"Cloned upstream Godot into '{self.target_dir}'.")
                self.stats["clone_action"] = "cloned"
                return True
            else:
                log_error(f"Target directory '{self.target_dir}' exists and is not empty, but is not a Git repository.")
                return False

        # Target directory is an existing git repository
        if self.dry_run:
            log_dry_run(f"Target repository exists at '{self.target_dir}'. Would fetch tag '{self.version}' and checkout.")
            self.stats["clone_action"] = "dry-run fetch"
            return True

        log_info(f"Target repository exists. Fetching tag/branch '{self.version}' from origin...")
        fetch_cmd = ["git", "fetch", "--depth", "1", "origin", "tag", self.version]
        res_fetch = self.run_command(fetch_cmd, cwd=self.target_dir)
        if res_fetch.returncode != 0:
            # Fallback to fetching ref directly
            fetch_cmd2 = ["git", "fetch", "--depth", "1", "origin", self.version]
            res_fetch2 = self.run_command(fetch_cmd2, cwd=self.target_dir)
            if res_fetch2.returncode != 0:
                log_warn(f"git fetch encountered warning/error (will attempt checkout):\n{res_fetch.stderr.strip()}")

        checkout_cmd = ["git", "-c", "advice.detachedHead=false", "checkout", self.version]
        res_checkout = self.run_command(checkout_cmd, cwd=self.target_dir)
        if res_checkout.returncode != 0:
            log_error(f"Failed to checkout version '{self.version}' in '{self.target_dir}':\n{res_checkout.stderr.strip()}")
            return False

        log_success(f"Checked out '{self.version}' in '{self.target_dir}'.")
        self.stats["clone_action"] = "fetched & checked out"
        return True

    def apply_patches(self) -> bool:
        """Scan and apply atomic patches in order using git apply --check and git apply --3way."""
        if self.skip_patch:
            log_info("Skipping patch application (--skip-patch).")
            return True

        if not self.target_dir.exists():
            if self.dry_run:
                log_dry_run(f"Target directory '{self.target_dir}' does not exist; skipping patch check in dry-run.")
                return True
            log_error(f"Target directory '{self.target_dir}' does not exist.")
            return False

        if not self.patches_dir.exists():
            log_error(f"Patches directory not found for version '{self.version}': '{self.patches_dir}'")
            return False

        patch_files = sorted(self.patches_dir.glob("*.patch"))
        if not patch_files:
            log_warn(f"No .patch files found in '{self.patches_dir}'.")
            return True

        log_info(f"Found {len(patch_files)} patch(es) in '{self.patches_dir}':")
        for p in patch_files:
            print(f"    - {p.name}")

        for patch in patch_files:
            patch_abs = str(patch.resolve())
            log_info(f"Processing patch: {patch.name}...")
            self.stats["patches_checked"] += 1

            # Step 1: Probe patch with git apply --check
            check_cmd = ["git", "apply", "--check", "--verbose", patch_abs]
            res_check = self.run_command(check_cmd, cwd=self.target_dir)

            if res_check.returncode == 0:
                # Check passed!
                if self.dry_run:
                    log_dry_run(f"Patch {patch.name} passed dry-run check (not applied).")
                    self.stats["patches_applied"] += 1
                    continue

                # Step 2: Apply patch with --3way
                apply_cmd = ["git", "apply", "--3way", patch_abs]
                res_apply = self.run_command(apply_cmd, cwd=self.target_dir)
                if res_apply.returncode == 0:
                    log_success(f"Applied patch: {patch.name}")
                    self.stats["patches_applied"] += 1
                else:
                    log_error(f"Failed to apply patch with --3way: {patch.name}")
                    conflicts = parse_git_apply_errors(res_apply.stderr)
                    if conflicts:
                        print(f"  {_colorize('Conflicts detected in:', COLOR_RED)}")
                        for c in conflicts:
                            print(f"    - File: {c['file']}, Line: {c['line']} ({c['reason']})")
                    print(f"  {_colorize('Git stderr:', COLOR_RED)}\n{res_apply.stderr.strip()}")
                    return False

            else:
                # git apply --check failed. Check if it is already applied (reverse check)
                rev_cmd = ["git", "apply", "-R", "--check", patch_abs]
                res_rev = self.run_command(rev_cmd, cwd=self.target_dir)
                if res_rev.returncode == 0:
                    log_skip(f"Patch {patch.name} is already applied in target repository.")
                    self.stats["patches_skipped"] += 1
                    continue

                # Actual conflict or application failure
                log_error(f"Patch validation failed: {patch.name}")
                conflicts = parse_git_apply_errors(res_check.stderr)
                if conflicts:
                    print(f"  {_colorize('Conflicts detected in:', COLOR_RED)}")
                    for c in conflicts:
                        print(f"    - File: {c['file']}, Line: {c['line']} ({c['reason']})")
                print(f"  {_colorize('Git stderr:', COLOR_RED)}\n{res_check.stderr.strip()}")
                return False

        return True

    def deploy_assets(self) -> bool:
        """Deploy OEM visual assets into the target directory."""
        if self.skip_assets:
            log_info("Skipping OEM visual assets deployment (--skip-assets).")
            return True

        if not self.target_dir.exists():
            if self.dry_run:
                log_dry_run(f"Target directory '{self.target_dir}' does not exist; skipping asset deployment check in dry-run.")
                return True
            log_error(f"Target directory '{self.target_dir}' does not exist.")
            return False

        if not self.assets_dir.exists():
            log_error(f"OEM assets directory does not exist: '{self.assets_dir}'")
            return False

        log_info(f"Deploying OEM visual assets from '{self.assets_dir}' to '{self.target_dir}'...")

        deployed_count = 0
        for root, _, files in os.walk(self.assets_dir):
            for file_name in files:
                src_path = Path(root) / file_name
                rel_path = src_path.relative_to(self.assets_dir)
                dst_path = self.target_dir / rel_path

                if self.dry_run:
                    log_dry_run(f"Would copy asset: {rel_path.as_posix()} -> {dst_path.as_posix()}")
                else:
                    dst_path.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(src_path, dst_path)
                    if self.verbose:
                        log_deploy(f"Copied: {rel_path.as_posix()} ({src_path.stat().st_size} bytes)")

                deployed_count += 1

        self.stats["assets_deployed"] = deployed_count
        if self.dry_run:
            log_dry_run(f"Dry-run: verified {deployed_count} OEM asset(s) ready for deployment.")
        else:
            log_success(f"Successfully deployed {deployed_count} OEM visual asset(s).")

        return True

    def run(self) -> bool:
        """Execute the entire upstream sync workflow."""
        start_time = time.time()
        print("=" * 65)
        print(f"  MindSCADA Godot Upstream Synchronizer")
        print(f"  Version:     {self.version}")
        print(f"  Target:      {self.target_dir}")
        print(f"  Patches Dir: {self.patches_dir}")
        print(f"  Assets Dir:  {self.assets_dir}")
        print(f"  Dry-Run:     {self.dry_run}")
        print("=" * 65)

        # Stage 1: Clone or fetch
        if not self.sync_clone():
            log_error("Upstream synchronization stage failed.")
            return False

        # Stage 2: Patch application
        if not self.apply_patches():
            log_error("Patch application stage failed.")
            return False

        # Stage 3: OEM Assets deployment
        if not self.deploy_assets():
            log_error("OEM asset deployment stage failed.")
            return False

        elapsed = time.time() - start_time
        print("-" * 65)
        log_success(f"Upstream synchronization completed successfully in {elapsed:.2f}s!")
        print(f"  Clone Action:     {self.stats['clone_action']}")
        print(f"  Patches Checked:  {self.stats['patches_checked']}")
        print(f"  Patches Applied:  {self.stats['patches_applied']}")
        print(f"  Patches Skipped:  {self.stats['patches_skipped']}")
        print(f"  Assets Deployed:  {self.stats['assets_deployed']}")
        print("-" * 65)
        return True


def create_parser() -> argparse.ArgumentParser:
    """Construct command-line argument parser."""
    parser = argparse.ArgumentParser(
        description="MindSCADA Godot Upstream Synchronizer and Patch Manager",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--version",
        default="4.7.2-stable",
        help="Target Godot engine version tag or branch name",
    )
    parser.add_argument(
        "--target-dir",
        default=None,
        help="Target working directory for Godot engine source (default: ../godot.<version>)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Perform git apply --check and preview actions without modifying files",
    )
    parser.add_argument(
        "--skip-clone",
        action="store_true",
        help="Skip git clone/fetch if target directory already exists",
    )
    parser.add_argument(
        "--skip-patch",
        action="store_true",
        help="Skip applying atomic patches",
    )
    parser.add_argument(
        "--skip-assets",
        action="store_true",
        help="Skip deploying OEM visual assets",
    )
    parser.add_argument(
        "--repo-url",
        default="https://github.com/godotengine/godot.git",
        help="Upstream Godot Git repository URL",
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable detailed verbose output for debugging",
    )
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    """CLI entrypoint."""
    parser = create_parser()
    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parent.parent

    target_path = Path(args.target_dir).resolve() if args.target_dir else None

    syncer = UpstreamSyncer(
        repo_root=repo_root,
        version=args.version,
        target_dir=target_path,
        dry_run=args.dry_run,
        skip_clone=args.skip_clone,
        skip_patch=args.skip_patch,
        skip_assets=args.skip_assets,
        repo_url=args.repo_url,
        verbose=args.verbose,
    )

    success = syncer.run()
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
