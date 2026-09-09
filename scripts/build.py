#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MindSCADA Engine Extension - Unified Cross-Platform Build Driver.

This script provides a unified cross-platform build orchestration tool replacing
legacy bat/sh build scripts (build.windows.all.bat, build.x11.*.sh).

Key features:
1. Auto-configures toolchains (MSVC via vswhere/vcvars64 on Windows, ARM toolchain/flags on Linux).
2. Auto-detects host platform, architecture, CPU concurrency, and Godot repository location.
3. Automatically mounts external custom_modules and exports MINDSCADA_DEPS_ROOT.
4. Injects custom module enablement flags and library version macros.
5. Supports dry-run execution (--dry-run) and matrix target builds (--all).

Usage:
    python scripts/build.py --platform <windows|linux> --arch <x86_64|arm32|arm64>
                            --target <editor|template_debug|template_release|all>
                            [--dev] [--godot-dir <path>] [--modules-dir <path>]
                            [--deps-dir <path>] [--jobs N] [--dry-run]
"""

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
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
# Host Environment Detection
# ---------------------------------------------------------------------------

def detect_host_platform() -> str:
    """Detect current host operating system ('windows' or 'linux')."""
    if sys.platform.startswith("win"):
        return "windows"
    return "linux"


def detect_host_arch() -> str:
    """Detect current host architecture ('x86_64', 'arm32', or 'arm64')."""
    mach = platform.machine().lower()
    if mach in ("arm64", "aarch64"):
        return "arm64"
    if "arm" in mach:
        return "arm32"
    if mach in ("amd64", "x86_64", "x64"):
        return "x86_64"
    return "x86_64"


def probe_repo_root() -> Path:
    """Resolve the root directory of the mindscada-engine-ext repository."""
    return safe_path(Path(__file__).parent.parent)


def probe_godot_dir(repo_root: Path, custom_path: Optional[str] = None) -> Path:
    """
    Probe the Godot engine source directory.
    Priority:
    1. Explicit custom_path if provided.
    2. Sibling directory ../godot.4.7.2 (if contains SConstruct or exists)
    3. Sibling directory ../godot
    4. Sibling directory ../godot.4.2.1
    5. Parent directory containing SConstruct
    6. Current working directory containing SConstruct
    7. Fallback: ../godot.4.7.2
    """
    if custom_path:
        return safe_path(custom_path)

    candidates = [
        repo_root.parent / "godot.4.7.2",
        repo_root.parent / "godot",
        repo_root.parent / "godot.4.2.1",
    ]

    for cand in candidates:
        if (cand / "SConstruct").is_file():
            return safe_path(cand)

    for cand in candidates:
        if cand.is_dir():
            return safe_path(cand)

    if (repo_root.parent / "SConstruct").is_file():
        return safe_path(repo_root.parent)

    if (Path.cwd() / "SConstruct").is_file():
        return safe_path(Path.cwd())

    return safe_path(repo_root.parent / "godot.4.7.2")


# ---------------------------------------------------------------------------
# Toolchain & Environment Initialization
# ---------------------------------------------------------------------------

def is_msvc_active() -> bool:
    """Check whether MSVC x64 compiler environment is already active in current process."""
    if os.environ.get("VCINSTALLDIR") or os.environ.get("VSCMD_VER"):
        if os.environ.get("VSCMD_ARG_TGT_ARCH") == "x64":
            return True
    return False


def find_vswhere() -> Optional[str]:
    """Find vswhere.exe executable path if available."""
    vsw = shutil.which("vswhere")
    if vsw and os.path.isfile(vsw):
        return vsw
    candidates = [
        r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe",
        r"C:\Program Files\Microsoft Visual Studio\Installer\vswhere.exe",
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return None


def find_vcvars64(custom_path: Optional[str] = None) -> Optional[str]:
    """
    Locate Visual Studio vcvars64.bat.
    Probes vswhere.exe output first, then falls back to preset VS 2022/2019 locations.
    """
    if custom_path:
        if os.path.isfile(custom_path):
            return str(Path(custom_path).resolve())
        return None

    # 1. Try vswhere.exe
    vswhere_exe = find_vswhere()
    if vswhere_exe:
        try:
            cmd = [
                vswhere_exe,
                "-latest",
                "-products", "*",
                "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property", "installationPath"
            ]
            res = subprocess.run(cmd, capture_output=True, text=True, errors="replace", check=True)
            vs_path = res.stdout.strip()
            if vs_path:
                vcvars = Path(vs_path) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
                if vcvars.is_file():
                    return str(vcvars.resolve())
        except Exception:
            pass

    # 2. Preset fallback locations
    presets = [
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
    ]
    for p in presets:
        if os.path.isfile(p):
            return str(Path(p).resolve())

    return None


def extract_vcvars_env(vcvars_bat: str) -> Dict[str, str]:
    """Execute vcvars64.bat in a shell and capture exported environment variables."""
    cmd = f'call "{vcvars_bat}" >nul 2>&1 && set'
    proc = subprocess.run(cmd, shell=True, capture_output=True, text=True, errors="replace", check=True)
    env: Dict[str, str] = {}
    for line in proc.stdout.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            env[k] = v
    return env


def ensure_gettext_in_path() -> None:
    """Ensure msgfmt / gettext binaries are present in PATH if available."""
    gettext_candidates = [
        Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "gettext-iconv" / "bin",
        Path(r"C:\Program Files\gettext-iconv\bin"),
        Path(r"C:\Program Files (x86)\gettext-iconv\bin"),
    ]
    for gc in gettext_candidates:
        if (gc / "msgfmt.exe").is_file():
            if str(gc) not in os.environ.get("PATH", ""):
                os.environ["PATH"] = f"{gc}{os.pathsep}{os.environ.get('PATH', '')}"
                log_info(f"Prepended gettext tools to PATH: {gc}")
            break


def setup_msvc_environment(dry_run: bool = False, custom_vcvars: Optional[str] = None) -> bool:
    """
    Ensure MSVC build environment is active.
    If not active, locate vcvars64.bat and inject variables into os.environ.
    """
    ensure_gettext_in_path()

    if is_msvc_active():
        log_info("MSVC environment is already active.")
        return True

    vcvars_bat = find_vcvars64(custom_vcvars)
    if not vcvars_bat:
        log_warn("MSVC vcvars64.bat not found. Compilation may fail if compiler is not in PATH.")
        return False

    log_info(f"Found MSVC toolchain activator: {vcvars_bat}")
    if dry_run:
        log_dry_run(f"Would invoke: call \"{vcvars_bat}\" to inject MSVC compiler environment")
        return True

    try:
        log_info("Activating MSVC x64 build environment...")
        new_env = extract_vcvars_env(vcvars_bat)
        os.environ.update(new_env)
        ensure_gettext_in_path()
        log_success(f"MSVC environment activated successfully (VCINSTALLDIR: {os.environ.get('VCINSTALLDIR', 'N/A')})")
        return True
    except Exception as e:
        log_error(f"Failed to activate MSVC environment via {vcvars_bat}: {e}")
        return False


def setup_arm_gcc_environment(arm_gcc_path: Optional[str] = None, dry_run: bool = False) -> Dict[str, str]:
    """
    Configure environment for Linux ARM32 / ARM64 cross-compilation.
    Prepends toolchain /bin to PATH and /lib to LD_LIBRARY_PATH if ARM_GCC_PATH is provided.
    """
    env_updates: Dict[str, str] = {}
    gcc_path = arm_gcc_path or os.environ.get("ARM_GCC_PATH")
    if not gcc_path:
        return env_updates

    resolved = Path(gcc_path).resolve()
    bin_dir = resolved / "bin" if (resolved / "bin").is_dir() else resolved
    lib_dir = resolved / "lib" if (resolved / "lib").is_dir() else None

    log_info(f"Configuring ARM toolchain path from: {resolved}")

    current_path = os.environ.get("PATH", "")
    if str(bin_dir) not in current_path:
        new_path = f"{bin_dir}{os.pathsep}{current_path}"
        env_updates["PATH"] = new_path
        if not dry_run:
            os.environ["PATH"] = new_path
        log_info(f"Prepended toolchain bin to PATH: {bin_dir}")

    if lib_dir:
        current_ld = os.environ.get("LD_LIBRARY_PATH", "")
        if str(lib_dir) not in current_ld:
            new_ld = f"{lib_dir}{os.pathsep}{current_ld}" if current_ld else str(lib_dir)
            env_updates["LD_LIBRARY_PATH"] = new_ld
            if not dry_run:
                os.environ["LD_LIBRARY_PATH"] = new_ld
            log_info(f"Prepended toolchain lib to LD_LIBRARY_PATH: {lib_dir}")

    return env_updates


# ---------------------------------------------------------------------------
# Target Matrix & Command Assembly
# ---------------------------------------------------------------------------

def get_build_plan(target: str, dev: bool, build_all: bool) -> List[Dict[str, Any]]:
    """
    Determine the list of build targets to execute.
    If build_all or target == 'all', returns all 4 core targets:
      1. editor dev
      2. editor release
      3. template_debug
      4. template_release
    If target == 'dist', returns the 3 distribution targets:
      1. editor release
      2. template_debug
      3. template_release
    Otherwise returns single specified target.
    """
    if build_all or target == "all":
        return [
            {"target": "editor", "dev": True, "name": "editor dev"},
            {"target": "editor", "dev": False, "name": "editor release"},
            {"target": "template_debug", "dev": False, "name": "template_debug"},
            {"target": "template_release", "dev": False, "name": "template_release"},
        ]
    if target == "dist":
        return [
            {"target": "editor", "dev": False, "name": "editor release"},
            {"target": "template_debug", "dev": False, "name": "template_debug"},
            {"target": "template_release", "dev": False, "name": "template_release"},
        ]
    return [
        {"target": target, "dev": dev, "name": f"{target} ({'dev' if dev else 'release'})"}
    ]


def probe_custom_module_deps(
    deps_dir: Path,
    platform_name: str,
    arch: str,
) -> Dict[str, bool]:
    """
    Probe whether third-party dependencies are available for each custom module.

    Checks three locations in priority order:
      1. deps/{platform}_{arch}/  (bundled in-repo dependencies)
      2. MINDSCADA_DEPS_ROOT env var (CI or external dependency root)
      3. Platform-specific local fallback paths (developer workstation)

    Returns a dict of module_name -> bool indicating availability.
    Modules 'webview' and 'qtwindow' always return True (they have built-in
    dummy/system fallbacks in their SCsub).
    """
    # Normalize platform/arch keys the same way SCsub does
    norm_plat = "linux" if platform_name in ("linux", "linuxbsd") else platform_name
    if "arm64" in arch or "aarch64" in arch:
        norm_arch = "arm64"
    elif "arm" in arch:
        norm_arch = "arm32"
    elif "64" in arch:
        norm_arch = "x64"
    elif "86" in arch or "32" in arch:
        norm_arch = "x86"
    else:
        norm_arch = arch

    platform_key = f"{norm_plat}_{norm_arch}"

    def _dir_has_content(p: Path) -> bool:
        """Return True if directory exists and has entries beyond .gitkeep."""
        if not p.is_dir():
            return False
        entries = [e for e in p.iterdir() if e.name != ".gitkeep"]
        return len(entries) > 0

    # 1. Check bundled deps/{platform}_{arch}/
    has_deps = _dir_has_content(deps_dir / platform_key)

    # 2. Check MINDSCADA_DEPS_ROOT
    if not has_deps:
        env_root = os.environ.get("MINDSCADA_DEPS_ROOT", "")
        if env_root:
            has_deps = _dir_has_content(Path(env_root) / platform_key)

    # 3. Check platform-specific local fallback paths (developer workstations)
    if not has_deps:
        if platform_name == "windows":
            for fb in [Path("C:/opensource"), Path("C:/lib")]:
                if fb.is_dir():
                    has_deps = True
                    break
        elif platform_name in ("linux", "linuxbsd"):
            for fb in [Path("/media/pi/A31C-5DF8")]:
                if fb.is_dir():
                    has_deps = True
                    break

    return {
        "kvmanager": has_deps,
        "varmanager": has_deps,
        "mqttmanager": has_deps,
        "webview": True,   # Has dummy fallback in SCsub
        "qtwindow": True,  # Uses system APIs only, no third-party deps
    }


def assemble_scons_args(
    platform_name: str,
    arch: str,
    target: str,
    dev: bool,
    modules_dir: Path,
    jobs: int,
    scons_bin: str = "scons",
    compiler_ver: Optional[str] = None,
    vsproj: bool = False,
    extra_args: Optional[List[str]] = None,
    cache_path: Optional[Path] = None,
    cache_limit: Optional[int] = None,
    deps_dir: Optional[Path] = None,
) -> List[str]:
    """
    Assemble the complete SCons command arguments list according to project specifications.

    Parameters:
      platform_name: 'windows' or 'linux'/'linuxbsd'
      arch: 'x86_64', 'arm32', 'arm64'
      target: 'editor', 'template_debug', 'template_release'
      dev: boolean flag for developer build
      modules_dir: Path to external custom_modules
      jobs: Concurrency jobs count
      scons_bin: Executable binary for SCons
      compiler_ver: Optional compiler version macro
      vsproj: Generate Visual Studio solution
      extra_args: Additional raw SCons arguments
      cache_path: Path for SCons compilation cache
      cache_limit: Cache size limit in GiB
      deps_dir: Path to third-party deps root for auto-detection of module availability
    """
    cmd = [scons_bin]

    # 1. Platform normalization
    norm_plat = "linuxbsd" if platform_name in ("linux", "linuxbsd") else "windows"
    cmd.append(f"platform={norm_plat}")

    # 2. Architecture
    cmd.append(f"arch={arch}")

    # 3. Target
    cmd.append(f"target={target}")

    # 4. Dev build & debug symbols
    if dev:
        cmd.append("dev_build=yes")
        cmd.append("debug_symbols=yes")
    else:
        cmd.append("dev_build=no")

    cmd.append("separate_debug_symbols=yes")

    # 5. Custom modules path (forward slashes for cross-platform robustness)
    cmd.append(f"custom_modules={modules_dir.as_posix()}")

    # 6. Core module enablement flags — auto-detect dependency availability
    cmd.append("module_mono_enabled=no")

    if deps_dir:
        module_avail = probe_custom_module_deps(deps_dir, platform_name, arch)
    else:
        # If no deps_dir provided, assume all modules are available (backward compat)
        module_avail = {m: True for m in ["kvmanager", "varmanager", "mqttmanager", "webview", "qtwindow"]}

    for mod_name in ["kvmanager", "varmanager", "mqttmanager", "webview", "qtwindow"]:
        enabled = module_avail.get(mod_name, True)
        flag_val = "yes" if enabled else "no"
        cmd.append(f"module_{mod_name}_enabled={flag_val}")
        if not enabled:
            log_warn(f"Module '{mod_name}' auto-disabled: third-party dependencies not found")

    # 7. Third-party dependency version macros
    cmd.append("OPEN62541VER=.v1.3.9")
    cmd.append("MBEDTLSVER=v2.28.4")
    cmd.append("MMKVVER=-1.3.3")
    cmd.append("MQTTVER=v1.4.1")

    # 8. Linux platform specific options
    if norm_plat == "linuxbsd":
        cmd.append("use_llvm=no")
        if arch == "arm32":
            # Hardware floating-point & Cortex-A72 optimization for pi32
            cmd.append("disable_exceptions=yes")
            cmd.append("CCFLAGS=-mcpu=cortex-a72 -mtune=cortex-a72 -mfpu=neon-fp-armv8 -mfloat-abi=hard -mlittle-endian -munaligned-access")
            cmd.append("CXXFLAGS=-std=c++17")

        if compiler_ver:
            norm_cver = compiler_ver if compiler_ver.startswith("-") else f"-{compiler_ver}"
            cmd.append(f"COMPILERVER={norm_cver}")
    else:
        # Windows platform options: disable external optional drivers if Agility SDK / AccessKit not configured
        cmd.append("d3d12=no")
        cmd.append("accesskit=no")
        cmd.append("linkflags=/FORCE:MULTIPLE")
        if compiler_ver:
            norm_cver = compiler_ver if compiler_ver.startswith("-") else f"-{compiler_ver}"
            cmd.append(f"COMPILERVER={norm_cver}")

    # 9. VS solution generation option (if requested)
    if vsproj and norm_plat == "windows":
        cmd.append("vsproj=yes")

    # 10. Concurrency jobs
    cmd.append(f"-j{jobs}")

    # 11. SCons build cache
    if cache_path:
        cmd.append(f"cache_path={cache_path.as_posix()}")
        if cache_limit and cache_limit > 0:
            cmd.append(f"cache_limit={cache_limit}")

    # 12. Extra custom arguments
    if extra_args:
        cmd.extend(extra_args)

    return cmd


def format_command_display(cmd: List[str]) -> str:
    """
    Format command list for human display and terminal copy-pasting.
    Properly wraps arguments containing spaces or macro values in shell quotes.
    """
    quoted_keys = {
        "custom_modules",
        "CCFLAGS",
        "CXXFLAGS",
        "OPEN62541VER",
        "MBEDTLSVER",
        "MMKVVER",
        "MQTTVER",
        "COMPILERVER",
    }
    formatted = []
    for item in cmd:
        if "=" in item:
            k, v = item.split("=", 1)
            if k in quoted_keys or " " in v or "\t" in v:
                clean_v = v.strip("\"'")
                formatted.append(f'{k}="{clean_v}"')
            else:
                formatted.append(item)
        elif " " in item or "\t" in item:
            formatted.append(f'"{item}"')
        else:
            formatted.append(item)
    return " ".join(formatted)


# ---------------------------------------------------------------------------
# CLI Parser
# ---------------------------------------------------------------------------

def create_parser() -> argparse.ArgumentParser:
    """Create and configure command-line argument parser."""
    host_plat = detect_host_platform()
    host_arch = detect_host_arch()
    default_jobs = os.cpu_count() or 4

    parser = argparse.ArgumentParser(
        description="MindSCADA Engine Extension - Unified Cross-Platform Build Driver.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Dry-run editor dev build on current platform:
  python scripts/build.py --dev --dry-run

  # Build editor for Windows x64:
  python scripts/build.py --platform windows --arch x86_64 --target editor

  # Build all 4 core targets for Linux ARM32 in dry-run mode:
  python scripts/build.py --platform linux --arch arm32 --all --dry-run

  # Build all 4 targets for current platform:
  python scripts/build.py --all
        """,
    )

    # Platform & Target Options
    parser.add_argument(
        "--platform",
        choices=["windows", "linux", "linuxbsd"],
        default=host_plat,
        help=f"Target platform (default: auto-detected host platform '{host_plat}')",
    )
    parser.add_argument(
        "--arch",
        choices=["x86_64", "arm32", "arm64", "x86_32"],
        default=host_arch,
        help=f"Target CPU architecture (default: auto-detected host arch '{host_arch}')",
    )
    parser.add_argument(
        "--target",
        choices=["editor", "template_debug", "template_release", "dist", "all"],
        default="editor",
        help="Target build type (default: 'editor', 'dist'=editor release+templates, 'all'=all 4 targets)",
    )
    parser.add_argument(
        "--dev",
        action="store_true",
        default=False,
        help="Enable developer build options (dev_build=yes debug_symbols=yes)",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        default=False,
        dest="build_all",
        help="One-click pipeline build for all 4 core targets (editor dev, editor release, template_debug, template_release)",
    )
    parser.add_argument(
        "--dist",
        action="store_true",
        default=False,
        help="One-click distribution build for 3 release targets (editor release, template_debug, template_release)",
    )

    # Directory Paths
    parser.add_argument(
        "--godot-dir",
        type=str,
        default=None,
        help="Path to Godot engine source root (default: auto-probed '../godot.4.7.2', '../godot', or '../godot.4.2.1')",
    )
    parser.add_argument(
        "--modules-dir",
        type=str,
        default=None,
        help="Path to custom modules directory (default: '<repo_root>/modules')",
    )
    parser.add_argument(
        "--deps-dir",
        type=str,
        default=None,
        help="Path to third-party dependencies root (default: '<repo_root>/deps')",
    )
    parser.add_argument(
        "--cache-path",
        type=str,
        default=None,
        help="SCons compilation cache directory (default: '<repo_root>/.cache/scons')",
    )
    parser.add_argument(
        "--cache-limit",
        type=int,
        default=20,
        help="SCons compilation cache size limit in GiB (default: 20 GiB, 0=unlimited)",
    )
    parser.add_argument(
        "--no-cache",
        action="store_true",
        default=False,
        help="Disable SCons compilation cache",
    )

    # Execution & Concurrency
    parser.add_argument(
        "-j", "--jobs",
        type=int,
        default=default_jobs,
        help=f"Concurrent compilation threads (default: CPU cores count = {default_jobs})",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=False,
        help="Only display commands and environment variables without invoking SCons",
    )
    parser.add_argument(
        "--continue-on-error",
        action="store_true",
        default=False,
        help="In pipeline mode (--all), continue remaining targets if one fails",
    )

    # Toolchain Configuration
    parser.add_argument(
        "--arm-gcc-path",
        type=str,
        default=os.environ.get("ARM_GCC_PATH"),
        help="Toolchain root path for Linux ARM cross-compilation",
    )
    parser.add_argument(
        "--vcvars-path",
        type=str,
        default=None,
        help="Explicit path to vcvars64.bat (Windows MSVC only)",
    )
    parser.add_argument(
        "--scons-bin",
        type=str,
        default="scons",
        help="SCons executable name or path (default: 'scons')",
    )
    parser.add_argument(
        "--compiler-ver",
        type=str,
        default=None,
        help="Compiler version macro (e.g. '-gcc11.3.1' for Linux ARM32)",
    )
    parser.add_argument(
        "--vsproj",
        action="store_true",
        default=False,
        help="Generate Visual Studio solution files (Windows only)",
    )
    parser.add_argument(
        "--extra-args",
        type=str,
        default=None,
        help="Extra SCons arguments passed as a string (e.g. --extra-args \"verbose=yes\")",
    )
    parser.add_argument(
        "trailing_args",
        nargs=argparse.REMAINDER,
        help="Additional raw arguments passed directly to SCons",
    )

    return parser


# ---------------------------------------------------------------------------
# Main Orchestrator
# ---------------------------------------------------------------------------

def run_build(args: argparse.Namespace) -> int:
    """Execute or simulate build driver process."""
    repo_root = probe_repo_root()
    godot_dir = probe_godot_dir(repo_root, args.godot_dir)
    modules_dir = safe_path(args.modules_dir) if args.modules_dir else safe_path(repo_root / "modules")
    deps_dir = safe_path(args.deps_dir) if args.deps_dir else safe_path(repo_root / "deps")

    # Collect extra arguments
    extra_args: List[str] = []
    if args.extra_args:
        extra_args.extend(args.extra_args.split())
    if args.trailing_args:
        for t in args.trailing_args:
            if t != "--":
                extra_args.append(t)

    # Resolve build target plan
    target_arg = "dist" if args.dist else args.target
    plan = get_build_plan(target_arg, args.dev, args.build_all)

    # SCons Cache Configuration
    cache_path: Optional[Path] = None
    if not args.no_cache:
        if args.cache_path:
            cache_path = safe_path(args.cache_path)
        else:
            cache_path = safe_path(repo_root / ".cache" / "scons")
        cache_path.mkdir(parents=True, exist_ok=True)

    # Print build summary header
    banner = "=" * 70
    print(banner)
    title_suffix = " (DRY-RUN)" if args.dry_run else ""
    log_info(f"MindSCADA Engine Extension - Build Driver{title_suffix}")
    print(banner)
    log_info(f"Host Environment   : OS={detect_host_platform()}, Arch={detect_host_arch()}, Cores={os.cpu_count()}")
    log_info(f"Target Platform    : {args.platform}")
    log_info(f"Target Architecture: {args.arch}")
    log_info(f"Godot Source Dir   : {godot_dir}")
    log_info(f"Modules Dir        : {modules_dir}")
    log_info(f"Dependencies Dir   : {deps_dir}")
    if cache_path:
        log_info(f"SCons Build Cache  : {cache_path} (limit={args.cache_limit} GiB)")
    else:
        log_info("SCons Build Cache  : Disabled")
    log_info(f"Parallel Jobs      : {args.jobs}")
    log_info(f"Build Plan Tasks   : {len(plan)} target(s)")
    for i, p in enumerate(plan, 1):
        print(f"   [{i}] {p['name']} (target={p['target']}, dev={p['dev']})")
    print(banner)

    # Export MINDSCADA_DEPS_ROOT environment variable
    deps_root_str = str(deps_dir)
    os.environ["MINDSCADA_DEPS_ROOT"] = deps_root_str
    log_info(f"Exported Environment Variable: MINDSCADA_DEPS_ROOT = {deps_root_str}")

    # Toolchain initialization
    norm_plat = "linuxbsd" if args.platform in ("linux", "linuxbsd") else "windows"

    if norm_plat == "windows":
        setup_msvc_environment(dry_run=args.dry_run, custom_vcvars=args.vcvars_path)
    elif norm_plat == "linuxbsd" and args.arch in ("arm32", "arm64"):
        setup_arm_gcc_environment(arm_gcc_path=args.arm_gcc_path, dry_run=args.dry_run)

    # In actual run mode, verify that godot_dir contains SConstruct
    if not args.dry_run:
        sconstruct = godot_dir / "SConstruct"
        if not sconstruct.is_file():
            log_error(f"Godot SConstruct not found at {sconstruct}")
            log_error("Please run scripts/sync_upstream.py to clone/sync Godot source, or specify --godot-dir.")
            return 1

    # Execute build plan
    total = len(plan)
    for idx, item in enumerate(plan, 1):
        target_name = item["target"]
        is_dev = item["dev"]
        display_name = item["name"]

        log_step(f"Target [{idx}/{total}]: {display_name}")

        scons_cmd = assemble_scons_args(
            platform_name=args.platform,
            arch=args.arch,
            target=target_name,
            dev=is_dev,
            modules_dir=modules_dir,
            jobs=args.jobs,
            scons_bin=args.scons_bin,
            compiler_ver=args.compiler_ver,
            vsproj=args.vsproj,
            extra_args=extra_args,
            cache_path=cache_path,
            cache_limit=args.cache_limit,
            deps_dir=deps_dir,
        )

        cmd_display = format_command_display(scons_cmd)

        if args.dry_run:
            log_dry_run(f"Working Directory: {godot_dir}")
            log_dry_run("Injected Environment:")
            log_dry_run(f"  MINDSCADA_DEPS_ROOT={deps_root_str}")
            if norm_plat == "linuxbsd" and args.arm_gcc_path:
                log_dry_run(f"  ARM_GCC_PATH={args.arm_gcc_path}")
            log_dry_run("SCons Command:")
            print(f"  {_colorize(cmd_display, COLOR_GREEN)}")
            continue

        log_info(f"Cwd: {godot_dir}")
        log_info(f"Exec: {cmd_display}")

        try:
            res = subprocess.run(scons_cmd, cwd=str(godot_dir), env=os.environ)
            if res.returncode != 0:
                log_error(f"Build failed for '{display_name}' with exit code {res.returncode}")
                if not args.continue_on_error:
                    return res.returncode
        except FileNotFoundError:
            log_error(f"SCons executable '{args.scons_bin}' not found in PATH.")
            log_error("Please ensure SCons is installed ('pip install scons') and added to PATH.")
            return 1
        except Exception as e:
            log_error(f"Execution error for '{display_name}': {e}")
            if not args.continue_on_error:
                return 1

    print(f"\n{banner}")
    if args.dry_run:
        log_success(f"Dry-run simulation completed successfully for all {total} target(s).")
    else:
        log_success(f"Build pipeline completed successfully for all {total} target(s).")
    print(banner)

    return 0


def main() -> int:
    parser = create_parser()
    args = parser.parse_args()
    return run_build(args)


if __name__ == "__main__":
    sys.exit(main())
