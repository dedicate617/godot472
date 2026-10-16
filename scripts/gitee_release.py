#!/usr/bin/env python3
"""
MindSCADA Engine Extension - Gitee Release & Asset Publisher
============================================================
Automates creating Gitee Releases and uploading packaged distribution
artifacts (.zip, .tpz, checksums.sha256) via the Gitee Open API (v5).

Requirements:
    - Python 3.6+ (Standard library only: no external dependencies required)
    - Gitee Personal Access Token (with 'projects' permission)

Usage:
    python scripts/gitee_release.py --token <TOKEN> --tag v4.7.2-20260923 --dist-dir dist/
"""

import argparse
import json
import mimetypes
import os
from pathlib import Path
import shutil
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
import uuid
from typing import Dict, List, Optional, Set, Tuple, Union


def log_info(msg: str) -> None:
    print(f"[INFO] {msg}", flush=True)


def log_success(msg: str) -> None:
    print(f"[SUCCESS] {msg}", flush=True)


def log_warn(msg: str) -> None:
    print(f"[WARNING] {msg}", flush=True)


def log_error(msg: str) -> None:
    print(f"[ERROR] {msg}", file=sys.stderr, flush=True)


def make_multipart_body(fields: Dict[str, str], file_field: str, file_path: Path) -> Tuple[bytes, str]:
    """Generate multipart/form-data request body using Python standard library."""
    boundary = f"----GiteeReleaseBoundary{uuid.uuid4().hex}"
    parts = []

    # Add text fields
    for key, value in fields.items():
        parts.append(f"--{boundary}\r\n".encode("utf-8"))
        parts.append(f'Content-Disposition: form-data; name="{key}"\r\n\r\n'.encode("utf-8"))
        parts.append(f"{value}\r\n".encode("utf-8"))

    # Add binary file
    mime_type = mimetypes.guess_type(str(file_path))[0] or "application/octet-stream"
    parts.append(f"--{boundary}\r\n".encode("utf-8"))
    parts.append(
        f'Content-Disposition: form-data; name="{file_field}"; filename="{file_path.name}"\r\n'.encode("utf-8")
    )
    parts.append(f"Content-Type: {mime_type}\r\n\r\n".encode("utf-8"))
    with open(file_path, "rb") as f:
        parts.append(f.read())
    parts.append(f"\r\n--{boundary}--\r\n".encode("utf-8"))

    body = b"".join(parts)
    content_type = f"multipart/form-data; boundary={boundary}"
    return body, content_type


def curl_api(endpoint: str, method: str = "GET", data: Optional[Dict] = None, token: str = "") -> Tuple[int, Optional[Dict]]:
    """Execute Gitee API request using system curl with automatic proxy and DNS fallback."""
    url = f"https://gitee.com/api/v5/{endpoint}"
    if token:
        sep = "&" if "?" in url else "?"
        url += f"{sep}access_token={token}"

    curl_bin = shutil.which("curl") or shutil.which("curl.exe") or "curl"
    # Prioritize bypassing local proxy for gitee.com so large payloads and direct DNS work cleanly
    extra_arg_options = [
        ["--noproxy", "gitee.com"],
        [],
    ]

    for extra_args in extra_arg_options:
        cmd = [curl_bin] + extra_args + [
            "-s",
            "-w", "\n%{http_code}",
            "-X", method,
        ]
        if data:
            cmd.extend(["-H", "Content-Type: application/json;charset=UTF-8", "-d", json.dumps(data)])
        cmd.append(url)

        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30)
            output = proc.stdout.strip()
            lines = output.rsplit("\n", 1)
            http_code = int(lines[1]) if len(lines) > 1 and lines[1].isdigit() else (200 if proc.returncode == 0 else 500)
            body_text = lines[0] if len(lines) > 1 else output
            try:
                parsed = json.loads(body_text) if body_text else None
                return http_code, parsed
            except Exception:
                return http_code, None
        except Exception:
            continue

    return 500, None


def get_existing_release(repo: str, tag: str, token: str) -> Optional[Dict]:
    """Query Gitee API for an existing release with the given tag."""
    code, data = curl_api(f"repos/{repo}/releases/tags/{tag}", method="GET", token=token)
    if code == 200 and data and "id" in data:
        return data
    return None


def create_release(repo: str, tag: str, name: str, body: str, token: str, prerelease: bool = False, target_commitish: str = "master") -> Optional[int]:
    """Create a new Gitee Release or return existing release ID."""
    existing = get_existing_release(repo, tag, token)
    if existing and "id" in existing:
        log_info(f"Release for tag '{tag}' already exists on Gitee (ID: {existing['id']}).")
        return existing["id"]

    payload = {
        "access_token": token,
        "tag_name": tag,
        "name": name,
        "body": body,
        "prerelease": prerelease,
        "target_commitish": target_commitish,
    }
    code, res = curl_api(f"repos/{repo}/releases", method="POST", data=payload, token=token)
    if code in (200, 201) and res and "id" in res:
        release_id = res["id"]
        log_success(f"Created Gitee release '{name}' ({tag}) with ID: {release_id}")
        return release_id

    # Fallback check
    existing = get_existing_release(repo, tag, token)
    if existing and "id" in existing:
        return existing["id"]
    return None


def update_release(repo: str, release_id: int, tag: str, name: str, body: str, token: str) -> bool:
    """Update release notes and title via Gitee PATCH API."""
    payload = {
        "tag_name": tag,
        "name": name,
        "body": body,
    }
    code, res = curl_api(f"repos/{repo}/releases/{release_id}", method="PATCH", data=payload, token=token)
    if code == 200:
        log_success(f"Updated Gitee release metadata for '{tag}' (ID: {release_id})")
        return True
    log_warn(f"Failed to update Gitee release metadata (HTTP {code})")
    return False


def prune_old_releases(repo: str, current_tag: str, keep_count: int, token: str) -> List[str]:
    """
    Retain only the most recent `keep_count` releases on Gitee (including current_tag).
    Automatically deletes older releases via Gitee API to stay strictly within the 1 GB attachment quota.
    """
    if keep_count <= 0:
        return []

    code, releases = curl_api(f"repos/{repo}/releases?page=1&per_page=50", method="GET", token=token)
    if code != 200 or not isinstance(releases, list):
        log_warn(f"Failed to query Gitee releases for pruning (HTTP {code}).")
        return []

    # Sort descending by creation date or id
    sorted_releases = sorted(
        releases,
        key=lambda r: (r.get("created_at") or "", r.get("id") or 0),
        reverse=True,
    )

    preserved_ids = set()
    for r in sorted_releases:
        if r.get("tag_name") == current_tag:
            preserved_ids.add(r.get("id"))
            break

    pruned_tags = []
    other_kept = 0
    max_others = max(0, keep_count - len(preserved_ids))

    for r in sorted_releases:
        r_id = r.get("id")
        r_tag = r.get("tag_name")
        if r_id in preserved_ids:
            continue

        if other_kept < max_others:
            preserved_ids.add(r_id)
            other_kept += 1
        else:
            log_info(f"Pruning older Gitee release '{r_tag}' (ID: {r_id}) to reclaim storage quota...")
            del_code, _ = curl_api(f"repos/{repo}/releases/{r_id}", method="DELETE", token=token)
            if del_code in (200, 204):
                log_success(f"Successfully pruned old release '{r_tag}' (ID: {r_id}) from Gitee.")
                pruned_tags.append(r_tag)
            else:
                log_warn(f"Failed to prune release '{r_tag}' (ID: {r_id}, HTTP {del_code}).")

    if pruned_tags:
        log_success(f"Rolling prune complete: removed {len(pruned_tags)} old release(s): {', '.join(pruned_tags)}.")
    else:
        log_info(f"No releases needed pruning. Preserved {len(preserved_ids)} recent release(s).")

    return pruned_tags


def get_release_asset_names(repo: str, release_id: int, token: str) -> List[str]:
    """Retrieve existing asset names on the release to avoid duplicate uploads."""
    code, data = curl_api(f"repos/{repo}/releases/{release_id}", method="GET", token=token)
    if code == 200 and data:
        return [a.get("name") for a in data.get("assets", []) if a.get("name")]
    return []


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
    elif "pi32_bundle.zip" in fn:
        return "Linux ARM32 嵌入式独立部署包 (完整系统运行时与服务)"
    elif "services_windows.zip" in fn:
        return "Windows 平台 SCADA 后台独立服务套件"
    elif "services_linux_pi.zip" in fn:
        return "Linux ARM 树莓派 SCADA 后台独立服务套件"
    elif "checksums.sha256" in fn:
        return "SHA256 资产校验和清单文件"
    return "MindSCADA 发行包资产"


def generate_dual_source_release_body(
    tag: str,
    gitee_repo: str,
    github_repo: str,
    assets: List[Path],
    attached_names: set,
) -> str:
    """Generate Markdown description with dual-source download guidance and SHA256 verification."""
    lines = [
        f"# MindSCADA Industrial Engine Release {tag}",
        "",
        f"> **版本标签**：`{tag}`  ",
        f"> **引擎架构**：`Godot 4.7.2` / `MindSCADA Engine`  ",
        f"> **国内直连**：[Gitee Releases](https://gitee.com/{gitee_repo}/releases/tag/{tag})  ",
        f"> **全球镜像**：[GitHub Releases](https://github.com/{github_repo}/releases/tag/{tag})  ",
        "",
        "---",
        "",
        "### 🌐 双源托管与分发策略说明",
        "",
        "1. **Gitee 国内高速源（单文件 ≤ 100MB）**：包含日常开发必备的桌面编辑器、WebAssembly 导出模板、后端微服务及校验清单，国内网络直连极速下载；",
        "2. **GitHub Releases 全球源（全量包）**：包含全部平台产物与完整嵌入式固件部署镜像（如超大包 `MindSCADA_v4.7.2_pi32_bundle.zip` > 100MB），无单文件与存储限额；",
        "3. **SHA256 安全校验**：双源归档文件完全一致，建议下载后通过 `certutil -hashfile <文件名> SHA256` 校验。",
        "",
        "---",
        "",
        "### 📦 产物下载与 SHA256 校验清单",
        "",
        "| 资产文件名 | 大小 | Gitee 下载 | GitHub Releases 镜像 | 说明 |",
        "| :--- | :--- | :--- | :--- | :--- |",
    ]

    for a in sorted(assets, key=lambda p: p.name.lower()):
        size_mb = a.stat().st_size / (1024 * 1024)
        desc = get_asset_description(a.name)
        github_url = f"https://github.com/{github_repo}/releases/download/{tag}/{a.name}"
        github_link = f"[GitHub 镜像]({github_url})"

        if a.name in attached_names:
            gitee_url = f"https://gitee.com/{gitee_repo}/releases/download/{tag}/{a.name}"
            gitee_link = f"[点击下载]({gitee_url})"
        elif size_mb >= 95.0:
            gitee_link = "*(超 100MB 限制，由 GitHub 托管)*"
        else:
            gitee_link = "*(额度超限，推荐 GitHub 镜像)*"

        lines.append(f"| `{a.name}` | {size_mb:.2f} MB | {gitee_link} | {github_link} | {desc} |")

    lines.extend([
        "",
        "---",
        "*MindSCADA 自动化分发流水线自动生成*",
    ])
    return "\n".join(lines)


def upload_asset(repo: str, release_id: int, file_path: Path, token: str) -> Union[bool, str]:
    """Upload a distribution file to Gitee Release attach_files API using fast direct curl or urllib."""
    file_size_mb = file_path.stat().st_size / (1024 * 1024)
    if file_size_mb >= 95.0:
        log_info(f"Skipping Gitee upload for {file_path.name} ({file_size_mb:.2f} MB >= 95MB limit). Hosted on GitHub Releases.")
        return "SIZE_EXCEEDED"

    url = f"https://gitee.com/api/v5/repos/{repo}/releases/{release_id}/attach_files?access_token={token}"
    log_info(f"Uploading asset to Gitee: {file_path.name} ({file_size_mb:.2f} MB)...")

    # Normalize file path to avoid UNC path issues with curl on Windows VMware shared folders
    import re
    file_str = re.sub(r'^[\\/]+vmware-host[\\/]Shared Folders[\\/]', 'Z:\\\\', str(file_path))
    file_path = Path(file_str)
    file_arg = file_str

    # Prefer system curl: try normal direct connection first, fallback to direct IP if needed
    curl_bin = shutil.which("curl") or shutil.which("curl.exe")
    if curl_bin:
        for attempt, extra_args in enumerate([
            ["--noproxy", "gitee.com"],
            [],
        ]):
            curl_cmd = [curl_bin] + extra_args + ["-s", "-F", f"file=@{file_arg}", url]
            try:
                proc = subprocess.run(curl_cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=1200)
                if proc.returncode == 0 and ('"browser_download_url"' in proc.stdout or '"url"' in proc.stdout):
                    log_success(f"Asset uploaded successfully: {file_path.name}")
                    return True
                elif any(kw in proc.stdout for kw in ["1 GB", "限额", "quota", "超出"]):
                    log_warn(f"Gitee repository attachment quota exceeded (1 GB limit): {proc.stdout[:150]}")
                    return "QUOTA_EXCEEDED"
                else:
                    log_warn(f"curl upload attempt {attempt+1} failed with code {proc.returncode}: {proc.stdout[:200] or proc.stderr[:200]}")
            except Exception as e:
                log_warn(f"curl invocation exception attempt {attempt+1}: {e}")

    # Fallback to urllib
    fields = {"access_token": token}
    body, content_type = make_multipart_body(fields, "file", file_path)
    req = urllib.request.Request(
        url,
        data=body,
        headers={
            "Content-Type": content_type,
            "User-Agent": "MindSCADA-Publisher",
        },
    )

    try:
        with urllib.request.urlopen(req, timeout=300) as resp:
            if resp.status in (200, 201):
                log_success(f"Asset uploaded successfully: {file_path.name}")
                return True
            else:
                log_warn(f"Upload response HTTP {resp.status} for {file_path.name}")
    except urllib.error.HTTPError as e:
        error_content = e.read().decode("utf-8", errors="replace")
        log_error(f"Failed to upload asset {file_path.name} (HTTP {e.code}): {error_content}")
    except Exception as e:
        log_error(f"Exception uploading asset {file_path.name}: {e}")

    return False


def collect_dist_files(dist_dir: Path) -> List[Path]:
    """Find all publishable archives in dist/ directory."""
    files = []
    if not dist_dir.is_dir():
        return files
    for pattern in ("*.zip", "*.tpz", "checksums.sha256"):
        files.extend(dist_dir.glob(pattern))
    return sorted(set(files), key=lambda p: p.name.lower())


def main() -> int:
    parser = argparse.ArgumentParser(
        description="MindSCADA Engine Extension - Gitee Release & Asset Publisher"
    )
    parser.add_argument(
        "--token",
        type=str,
        default=os.environ.get("GITEE_TOKEN"),
        help="Gitee Personal Access Token (defaults to GITEE_TOKEN env var)",
    )
    parser.add_argument(
        "--repo",
        type=str,
        default=os.environ.get("GITEE_REPO", "bosmutus/mind-scada-release"),
        help="Target Gitee repository in 'owner/repo' format (default: 'bosmutus/mind-scada-release')",
    )
    parser.add_argument(
        "--github-repo",
        type=str,
        default="dedicate617/godot472",
        help="GitHub mirror repository in 'owner/repo' format (default: 'dedicate617/godot472')",
    )
    parser.add_argument(
        "--prune-old-releases",
        type=int,
        default=2,
        help="Number of recent releases to keep on Gitee (older releases pruned to stay under 1 GB, default: 2)",
    )
    parser.add_argument(
        "--tag",
        type=str,
        required=True,
        help="Release tag name (e.g. 'v4.7.2-20260923')",
    )
    parser.add_argument(
        "--name",
        type=str,
        default=None,
        help="Release title (default: 'MindSCADA Engine <tag>')",
    )
    parser.add_argument(
        "--body",
        type=str,
        default=None,
        help="Release notes text",
    )
    parser.add_argument(
        "--dist-dir",
        type=str,
        default="dist",
        help="Directory containing packaged distribution artifacts (default: 'dist')",
    )
    parser.add_argument(
        "--prerelease",
        action="store_true",
        default=False,
        help="Mark this release as a pre-release",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=False,
        help="Simulate release creation and asset uploading without network requests",
    )

    args = parser.parse_args()

    token = args.token
    if not token:
        token_file = Path(__file__).parent.parent / ".gitee_token"
        if token_file.is_file():
            token = token_file.read_text(encoding="utf-8").strip()

    if not token and not args.dry_run:
        log_error("Gitee access token is required. Pass --token or set GITEE_TOKEN environment variable.")
        log_info("To generate a token, visit: https://gitee.com/profile/personal_access_tokens (needs 'projects' scope)")
        return 1

    repo = args.repo.strip()
    tag = args.tag.strip()
    title = args.name or f"MindSCADA Engine {tag}"

    dist_path = Path(args.dist_dir)
    assets = sorted(collect_dist_files(dist_path), key=lambda f: f.stat().st_size)

    banner = "=" * 70
    print(banner)
    log_info("MindSCADA Engine Extension - Gitee Release Publisher")
    print(banner)
    log_info(f"Target Gitee Repo  : {repo}")
    log_info(f"GitHub Mirror Repo : {args.github_repo}")
    log_info(f"Release Tag        : {tag}")
    log_info(f"Release Title      : {title}")
    log_info(f"Prune Keep Count   : {args.prune_old_releases} recent releases")
    log_info(f"Distribution Dir   : {dist_path} ({len(assets)} asset(s) found)")
    for a in assets:
        size_mb = a.stat().st_size / (1024 * 1024)
        print(f"   + {a.name} ({size_mb:.2f} MB)")
    print(banner)

    if args.dry_run:
        log_info("[DRY-RUN] Would prune old releases beyond keep count:")
        log_info(f"[DRY-RUN] Target Keep Count: {args.prune_old_releases}")
        log_info("[DRY-RUN] Would create Gitee release and upload assets:")
        log_info(f"[DRY-RUN] URL: https://gitee.com/api/v5/repos/{repo}/releases")
        for a in assets:
            if a.stat().st_size / (1024 * 1024) >= 95.0:
                log_info(f"[DRY-RUN] Would skip upload to Gitee (size >= 95MB): {a.name} -> Hosted on GitHub Releases")
            else:
                log_info(f"[DRY-RUN] Would upload: {a.name}")
        log_success("[DRY-RUN] Simulation completed successfully.")
        return 0

    # 1. Prune older releases to free attachment quota
    if args.prune_old_releases > 0:
        log_info(f"Checking Gitee repository attachment quota (keeping latest {args.prune_old_releases} releases)...")
        prune_old_releases(repo, tag, args.prune_old_releases, token)

    # 2. Create or get Gitee release
    initial_desc = args.body or f"MindSCADA Industrial Engine Release {tag}\n\nBuilt with MindSCADA custom modules and patches."
    release_id = create_release(repo, tag, title, initial_desc, token, prerelease=args.prerelease)
    if not release_id:
        log_error("Aborting asset upload because Gitee release could not be resolved.")
        return 1

    # 3. Upload assets
    existing_asset_names = set(get_release_asset_names(repo, release_id, token))
    if existing_asset_names:
        log_info(f"Found {len(existing_asset_names)} asset(s) already attached on Gitee release.")

    uploaded_on_gitee = set(existing_asset_names)
    success_count = 0
    fail_count = 0
    quota_reached = False
    for a in assets:
        if a.name in existing_asset_names:
            log_info(f"Asset already attached to release, skipping: {a.name}")
            success_count += 1
            uploaded_on_gitee.add(a.name)
            continue
        res = upload_asset(repo, release_id, a, token)
        if res is True:
            success_count += 1
            uploaded_on_gitee.add(a.name)
        elif res == "SIZE_EXCEEDED":
            log_info(f"Asset {a.name} will be distributed via GitHub Releases mirror.")
        elif res == "QUOTA_EXCEEDED":
            quota_reached = True
            log_warn("Halting further asset uploads because Gitee 1 GB repository attachment limit has been reached.")
            break
        else:
            fail_count += 1

    # 4. Update Gitee Release Body with Dual-Source Guidance and Verified Asset Table
    log_info("Updating Gitee Release description with dual-source download manifest...")
    final_body = generate_dual_source_release_body(tag, repo, args.github_repo, assets, uploaded_on_gitee)
    update_release(repo, release_id, tag, title, final_body, token)

    print(banner)
    log_info(f"Gitee Release publishing completed: {success_count} succeeded, {fail_count} failed.")
    if quota_reached:
        log_warn("Note: Gitee free repository attachment quota (1 GB) is full. Assets are available on GitHub Releases.")
    log_info(f"View release at: https://gitee.com/{repo}/releases/tag/{tag}")
    print(banner)

    return 0 if (fail_count == 0 or quota_reached) else 1


if __name__ == "__main__":
    sys.exit(main())
