#!/usr/bin/env python3
"""
MindSCADA Engine Extension - Release Assets Publisher for Gitee
===============================================================
Publishes ONLY final compiled release assets (.zip, .tpz, checksums.sha256)
to the dedicated Gitee release repository (mind-scada-release.git).

Workflow:
  1. Scans dist/ for compiled artifacts.
  2. Syncs/clones Gitee release repository into a local staging area.
  3. Copies release packages and generates a detailed manifest README.md.
  4. Commits and tags the release assets in the release repository.
  5. Pushes master branch and release tag to Gitee.
  6. Optionally uploads assets to Gitee Web Releases if GITEE_TOKEN is available.

Usage:
  python scripts/publish_release_assets.py --tag v4.7.2-20260923 [--dry-run]
"""

import argparse
import datetime
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Dict, List, Optional, Tuple
import urllib.parse



def log_info(msg: str) -> None:
    print(f"[INFO] {msg}", flush=True)


def log_success(msg: str) -> None:
    print(f"[SUCCESS] {msg}", flush=True)


def log_warn(msg: str) -> None:
    print(f"[WARNING] {msg}", flush=True)


def log_error(msg: str) -> None:
    print(f"[ERROR] {msg}", file=sys.stderr, flush=True)


def get_git_credentials(host: str = "gitee.com") -> Tuple[Optional[str], Optional[str]]:
    """Query git credential helper for stored username and password."""
    try:
        proc = subprocess.run(
            ["git", "credential", "fill"],
            input=f"protocol=https\nhost={host}\n\n",
            capture_output=True,
            text=True,
            check=True,
        )
        username = None
        password = None
        for line in proc.stdout.splitlines():
            if line.startswith("username="):
                username = line.split("=", 1)[1].strip()
            elif line.startswith("password="):
                password = line.split("=", 1)[1].strip()
        return username, password
    except Exception:
        return None, None


def compute_sha256(filepath: Path) -> str:
    """Compute SHA256 hex digest for a file."""
    hasher = hashlib.sha256()
    with open(filepath, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            hasher.update(chunk)
    return hasher.hexdigest()


def get_asset_description(filename: str) -> str:
    """Return friendly description for known release asset filenames."""
    fn = filename.lower()
    if "windows_x86_64.zip" in fn:
        return "Windows x64 MindStudio 编辑器与 WebView2 运行时完整包"
    elif "export_templates_windows.tpz" in fn:
        return "Windows x64 导出模板 (包含 Release 与 Debug 模板及控制台版本)"
    elif "export_templates_web.tpz" in fn:
        return "Web WASM32 多线程导出模板 (包含 Release 与 Debug 模板)"
    elif "web_bundle.zip" in fn:
        return "Web WASM32 独立静态站点预览包 (直接部署 Nginx/HTTP 服务器)"
    elif "export_templates_pi32.tpz" in fn:
        return "Linux ARM32 (树莓派 32位系统) 导出模板"
    elif "export_templates_pi64.tpz" in fn:
        return "Linux ARM64 (树莓派 64位系统) 导出模板"
    elif "checksums.sha256" in fn:
        return "SHA256 资产校验和清单文件"
    return "MindSCADA 发行包资产"


def generate_release_readme(tag: str, version: str, assets: List[Tuple[Path, str, float]]) -> str:
    """Generate Markdown documentation for the release repository."""
    today = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = [
        "# MindSCADA Engine - 编译产物发布仓库",
        "",
        f"> **版本标签**：`{tag}`  ",
        f"> **引擎版本**：`Godot {version}` / `MindSCADA v{version}`  ",
        f"> **发布时间**：`{today}`  ",
        "",
        "本项目专门用于托管 **MindSCADA 工业 SCADA/工控引擎** 的最终编译 Release 资产。本仓库仅包含各平台的预编译可执行文件、导出模板与部署归档，不包含引擎核心源码。",
        "",
        "---",
        "",
        "## 包含的编译 Release 资产清单",
        "",
        "| 文件名 | 文件大小 | SHA256 校验和 | 资产说明 |",
        "| :--- | :--- | :--- | :--- |",
    ]

    for path, sha256_hash, size_mb in assets:
        short_hash = sha256_hash[:16] + "..."
        desc = get_asset_description(path.name)
        lines.append(f"| `{path.name}` | {size_mb:.2f} MB | `{short_hash}` | {desc} |")

    lines.extend([
        "",
        "---",
        "",
        "## 导出模板安装方法",
        "",
        "1. 启动 **MindStudio**（Windows 编辑器）；",
        "2. 点击顶部菜单：**项目 (Project)** → **导出 (Export)** → **管理导出模板 (Manage Export Templates)**；",
        "3. 点击 **从文件安装 (Install from file)**，选中对应的 `.tpz` 文件即可完成模板加载；",
        "4. 支持直接导出为 Windows 桌面程序、Linux 嵌入式程序或 WebAssembly 网页应用。",
        "",
        "---",
        "",
        "## Web 资产部署指南",
        "",
        "解压 `MindSCADA_v*_Web_bundle.zip` 后即可获得包含 `mindscada.html`, `mindscada.js`, `mindscada.wasm` 等文件的静态站点。由于开启了多线程（`SharedArrayBuffer`），Web 服务器必须配置以下响应头：",
        "```nginx",
        "add_header Cross-Origin-Opener-Policy same-origin;",
        "add_header Cross-Origin-Embedder-Policy require-corp;",
        "```",
        "",
        "---",
        f"*由 MindSCADA 自动化发布工具自动生成于 {today}*",
        "",
    ])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Publish compiled release assets to Gitee mind-scada-release repository"
    )
    parser.add_argument(
        "--tag",
        type=str,
        required=True,
        help="Release tag name (e.g. 'v4.7.2-20260923')",
    )
    parser.add_argument(
        "--version",
        type=str,
        default="4.7.2",
        help="Version string (default: '4.7.2')",
    )
    parser.add_argument(
        "--dist-dir",
        type=str,
        default="dist",
        help="Directory containing compiled release packages (default: 'dist')",
    )
    parser.add_argument(
        "--repo-url",
        type=str,
        default="https://gitee.com/bosmutus/mind-scada-release.git",
        help="Target Gitee release repository URL (default: 'https://gitee.com/bosmutus/mind-scada-release.git')",
    )
    parser.add_argument(
        "--staging-dir",
        type=str,
        default="scratch/mind-scada-release",
        help="Local staging directory for the release repo (default: 'scratch/mind-scada-release')",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=False,
        help="Simulate operations without committing or pushing",
    )

    args = parser.parse_args()

    tag = args.tag.strip()
    if not tag.startswith("v"):
        tag = f"v{tag}"

    version = args.version.strip()
    dist_dir = Path(args.dist_dir).resolve()
    staging_dir = Path(args.staging_dir).resolve()

    # 1. Scan dist files
    assets_raw = []
    for pattern in ("*.zip", "*.tpz", "checksums.sha256"):
        assets_raw.extend(dist_dir.glob(pattern))

    assets_raw = sorted(set(assets_raw), key=lambda p: p.name.lower())
    if not assets_raw:
        log_error(f"No release packages (*.zip, *.tpz) found in {dist_dir}!")
        log_error("Please run build_all.bat first to compile release assets.")
        return 1

    banner = "=" * 70
    print(banner)
    log_info("MindSCADA Engine - Release Assets Publisher (Gitee Dedicated)")
    print(banner)
    log_info(f"Target Tag         : {tag}")
    log_info(f"Target Repository  : {args.repo_url}")
    log_info(f"Source Dist Dir    : {dist_dir}")
    log_info(f"Local Staging Dir  : {staging_dir}")
    log_info(f"Release Assets ({len(assets_raw)} file(s)):")

    assets_info: List[Tuple[Path, str, float]] = []
    for p in assets_raw:
        sha256_hash = compute_sha256(p)
        size_mb = p.stat().st_size / (1024 * 1024)
        assets_info.append((p, sha256_hash, size_mb))
        print(f"   + {p.name:<45} ({size_mb:6.2f} MB)  SHA256: {sha256_hash[:16]}...")
    print(banner)

    if args.dry_run:
        log_info("[DRY-RUN] Would perform the following actions:")
        log_info(f"[DRY-RUN] 1. Clone/update {args.repo_url} in {staging_dir}")
        log_info(f"[DRY-RUN] 2. Copy {len(assets_info)} compiled release assets into staging directory")
        log_info(f"[DRY-RUN] 3. Generate release README.md with SHA256 verification table")
        log_info(f"[DRY-RUN] 4. git add . && git commit -m 'Release {tag}: compiled assets'")
        log_info(f"[DRY-RUN] 5. git tag -a {tag} -m 'Release {tag}'")
        log_info(f"[DRY-RUN] 6. git push origin master && git push origin {tag}")
        log_success("[DRY-RUN] Simulation completed successfully.")
        return 0

    # 2. Check / clone staging repo
    username, password = get_git_credentials("gitee.com")
    auth_url = args.repo_url
    if username and password and "://" in args.repo_url:
        proto, rest = args.repo_url.split("://", 1)
        safe_user = urllib.parse.quote(username, safe="")
        safe_pass = urllib.parse.quote(password, safe="")
        auth_url = f"{proto}://{safe_user}:{safe_pass}@{rest}"

    if not (staging_dir / ".git").is_dir():
        log_info(f"Cloning release repository into {staging_dir}...")
        staging_dir.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "clone", auth_url, str(staging_dir)], check=True)
    else:
        log_info(f"Updating existing release repository in {staging_dir}...")
        subprocess.run(["git", "-C", str(staging_dir), "remote", "set-url", "origin", auth_url], check=True)
        subprocess.run(["git", "-C", str(staging_dir), "checkout", "master"], check=False)
        subprocess.run(["git", "-C", str(staging_dir), "pull", "--rebase"], check=False)

    # 3. Copy assets into staging repository
    log_info("Copying release assets into staging repository...")
    for src, _, _ in assets_info:
        dst = staging_dir / src.name
        log_info(f"   -> Copying {src.name}...")
        shutil.copy2(src, dst)

    # 4. Generate README.md
    readme_content = generate_release_readme(tag, version, assets_info)
    (staging_dir / "README.md").write_text(readme_content, encoding="utf-8")
    log_success("Generated release README.md with download & verification table.")

    # 5. Git commit & tag in release repository
    log_info("Staging and committing release assets...")
    subprocess.run(["git", "-C", str(staging_dir), "add", "."], check=True)

    status_proc = subprocess.run(
        ["git", "-C", str(staging_dir), "status", "--porcelain"],
        capture_output=True,
        text=True,
        check=True,
    )
    if status_proc.stdout.strip():
        commit_msg = f"Release {tag}: update MindSCADA compiled distribution assets"
        subprocess.run(["git", "-C", str(staging_dir), "commit", "-m", commit_msg], check=True)
        log_success(f"Committed release assets in staging repo: '{commit_msg}'")
    else:
        log_info("No asset changes detected since last commit.")

    # 6. Create release tag
    tag_check = subprocess.run(
        ["git", "-C", str(staging_dir), "rev-parse", tag],
        capture_output=True,
    )
    if tag_check.returncode == 0:
        log_info(f"Tag '{tag}' already exists in release repository, replacing tag...")
        subprocess.run(["git", "-C", str(staging_dir), "tag", "-d", tag], check=True)

    subprocess.run(
        ["git", "-C", str(staging_dir), "tag", "-a", tag, "-m", f"MindSCADA Release {tag}"],
        check=True,
    )
    log_success(f"Tagged release assets with '{tag}'")

    # 7. Push to Gitee
    log_info(f"Pushing release assets and tag to Gitee ({args.repo_url})...")
    subprocess.run(["git", "-C", str(staging_dir), "push", "origin", "master"], check=True)
    subprocess.run(["git", "-C", str(staging_dir), "push", "origin", tag, "--force"], check=True)
    log_success(f"Successfully pushed release assets to Gitee ({args.repo_url})!")

    print(banner)
    log_success(f"All release assets published to Gitee successfully for tag {tag}!")
    log_info(f"View Gitee repository: {args.repo_url.replace('.git', '')}")
    log_info(f"View Gitee tag: {args.repo_url.replace('.git', '')}/tree/{tag}")
    print(banner)

    return 0


if __name__ == "__main__":
    sys.exit(main())
