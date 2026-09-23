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
from typing import Dict, List, Optional, Tuple


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
    """Execute Gitee API request using system curl with domestic direct IP resolve."""
    url = f"https://gitee.com/api/v5/{endpoint}"
    if token:
        sep = "&" if "?" in url else "?"
        url += f"{sep}access_token={token}"

    curl_bin = shutil.which("curl") or shutil.which("curl.exe") or "curl"
    cmd = [
        curl_bin,
        "--resolve", "gitee.com:443:180.76.198.77",
        "--noproxy", "*",
        "-s",
        "-w", "\n%{http_code}",
        "-X", method,
    ]
    if data:
        cmd.extend(["-H", "Content-Type: application/json;charset=UTF-8", "-d", json.dumps(data)])
    cmd.append(url)

    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        output = proc.stdout.strip()
        lines = output.rsplit("\n", 1)
        http_code = int(lines[1]) if len(lines) > 1 and lines[1].isdigit() else (200 if proc.returncode == 0 else 500)
        body_text = lines[0] if len(lines) > 1 else output
        try:
            return http_code, json.loads(body_text)
        except Exception:
            return http_code, None
    except Exception as e:
        log_warn(f"curl_api exception: {e}")
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


def get_release_asset_names(repo: str, release_id: int, token: str) -> List[str]:
    """Retrieve existing asset names on the release to avoid duplicate uploads."""
    code, data = curl_api(f"repos/{repo}/releases/{release_id}", method="GET", token=token)
    if code == 200 and data:
        return [a.get("name") for a in data.get("assets", []) if a.get("name")]
    return []


def upload_asset(repo: str, release_id: int, file_path: Path, token: str) -> bool:
    """Upload a distribution file to Gitee Release attach_files API using fast direct curl or urllib."""
    url = f"https://gitee.com/api/v5/repos/{repo}/releases/{release_id}/attach_files?access_token={token}"
    file_size_mb = file_path.stat().st_size / (1024 * 1024)
    log_info(f"Uploading asset to Gitee: {file_path.name} ({file_size_mb:.2f} MB)...")

    # Normalize file path to avoid UNC path issues with curl on Windows VMware shared folders
    file_str = str(file_path)
    if file_str.startswith("\\\\vmware-host\\Shared Folders\\D\\"):
        file_str = file_str.replace("\\\\vmware-host\\Shared Folders\\D\\", "Z:\\D\\")
    elif file_str.startswith("\\\\vmware-host\\Shared Folders\\"):
        file_str = file_str.replace("\\\\vmware-host\\Shared Folders\\", "Z:\\")

    try:
        rel = file_path.relative_to(Path.cwd())
        file_arg = str(rel)
    except Exception:
        file_arg = file_str

    # Prefer system curl if available: streams from disk and bypasses proxy with direct IP
    curl_bin = shutil.which("curl") or shutil.which("curl.exe")
    if curl_bin:
        curl_cmd = [
            curl_bin,
            "--resolve", "gitee.com:443:180.76.198.77",
            "--noproxy", "*",
            "-s",
            "-F", f"file=@{file_arg}",
            url,
        ]
        try:
            proc = subprocess.run(curl_cmd, capture_output=True, text=True, timeout=1200)
            if proc.returncode == 0 and ('"browser_download_url"' in proc.stdout or '"url"' in proc.stdout):
                log_success(f"Asset uploaded successfully: {file_path.name}")
                return True
            else:
                log_warn(f"curl upload failed with code {proc.returncode}: {proc.stdout[:200] or proc.stderr[:200]}")
        except Exception as e:
            log_warn(f"curl invocation exception: {e}")

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
    desc = args.body or f"MindSCADA Industrial Engine Release {tag}\n\nBuilt with MindSCADA custom modules and patches."

    dist_path = Path(args.dist_dir).resolve()
    assets = collect_dist_files(dist_path)

    banner = "=" * 70
    print(banner)
    log_info("MindSCADA Engine Extension - Gitee Release Publisher")
    print(banner)
    log_info(f"Target Gitee Repo  : {repo}")
    log_info(f"Release Tag        : {tag}")
    log_info(f"Release Title      : {title}")
    log_info(f"Distribution Dir   : {dist_path} ({len(assets)} asset(s) found)")
    for a in assets:
        size_mb = a.stat().st_size / (1024 * 1024)
        print(f"   + {a.name} ({size_mb:.2f} MB)")
    print(banner)

    if args.dry_run:
        log_info("[DRY-RUN] Would create Gitee release and upload assets:")
        log_info(f"[DRY-RUN] URL: https://gitee.com/api/v5/repos/{repo}/releases")
        for a in assets:
            log_info(f"[DRY-RUN] Would upload: {a.name}")
        log_success("[DRY-RUN] Simulation completed successfully.")
        return 0

    # 1. Create or get Gitee release
    release_id = create_release(repo, tag, title, desc, token, prerelease=args.prerelease)
    if not release_id:
        log_error("Aborting asset upload because Gitee release could not be resolved.")
        return 1

    # 2. Upload assets
    existing_asset_names = set(get_release_asset_names(repo, release_id, token))
    if existing_asset_names:
        log_info(f"Found {len(existing_asset_names)} asset(s) already attached on Gitee release.")

    success_count = 0
    fail_count = 0
    for a in assets:
        if a.name in existing_asset_names:
            log_info(f"Asset already attached to release, skipping: {a.name}")
            success_count += 1
            continue
        ok = upload_asset(repo, release_id, a, token)
        if ok:
            success_count += 1
        else:
            fail_count += 1

    print(banner)
    log_info(f"Gitee Release publishing completed: {success_count} succeeded, {fail_count} failed.")
    log_info(f"View release at: https://gitee.com/{repo}/releases/tag/{tag}")
    print(banner)

    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
