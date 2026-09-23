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


def get_existing_release(repo: str, tag: str, token: str) -> Optional[Dict]:
    """Query Gitee API for an existing release with the given tag."""
    url = f"https://gitee.com/api/v5/repos/{repo}/releases/tags/{tag}?access_token={token}"
    req = urllib.request.Request(url, headers={"User-Agent": "MindSCADA-Publisher"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            if resp.status == 200:
                data = json.loads(resp.read().decode("utf-8"))
                return data
    except urllib.error.HTTPError as e:
        if e.code == 404:
            return None
        log_warn(f"Error checking existing Gitee release for tag {tag}: HTTP {e.code}")
    except Exception as e:
        log_warn(f"Network error checking Gitee release: {e}")
    return None


def create_release(repo: str, tag: str, name: str, body: str, token: str, prerelease: bool = False) -> Optional[int]:
    """Create a new Gitee Release or return existing release ID."""
    existing = get_existing_release(repo, tag, token)
    if existing and "id" in existing:
        log_info(f"Release for tag '{tag}' already exists on Gitee (ID: {existing['id']}).")
        return existing["id"]

    url = f"https://gitee.com/api/v5/repos/{repo}/releases"
    payload = {
        "access_token": token,
        "tag_name": tag,
        "name": name,
        "body": body,
        "prerelease": prerelease,
    }
    data_bytes = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data_bytes,
        headers={
            "Content-Type": "application/json;charset=UTF-8",
            "User-Agent": "MindSCADA-Publisher",
        },
    )

    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            res = json.loads(resp.read().decode("utf-8"))
            release_id = res.get("id")
            log_success(f"Created Gitee release '{name}' ({tag}) with ID: {release_id}")
            return release_id
    except urllib.error.HTTPError as e:
        error_content = e.read().decode("utf-8", errors="replace")
        log_error(f"Failed to create Gitee release (HTTP {e.code}): {error_content}")
        # Retry querying existing in case of race condition
        existing = get_existing_release(repo, tag, token)
        if existing and "id" in existing:
            return existing["id"]
    except Exception as e:
        log_error(f"Exception creating Gitee release: {e}")
    return None


def upload_asset(repo: str, release_id: int, file_path: Path, token: str) -> bool:
    """Upload a distribution file to Gitee Release attach_files API."""
    url = f"https://gitee.com/api/v5/repos/{repo}/releases/{release_id}/attach_files?access_token={token}"
    fields = {"access_token": token}
    
    file_size_mb = file_path.stat().st_size / (1024 * 1024)
    log_info(f"Uploading asset to Gitee: {file_path.name} ({file_size_mb:.2f} MB)...")

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
                res = json.loads(resp.read().decode("utf-8"))
                download_url = res.get("browser_download_url") or res.get("url")
                log_success(f"Asset uploaded successfully: {file_path.name} -> {download_url or 'OK'}")
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
    success_count = 0
    fail_count = 0
    for a in assets:
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
