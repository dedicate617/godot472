#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Download and extract latest GitHub Actions CI/CD artifacts to dist/ directory.
"""

import os
import sys
import io
import json
import zipfile
import hashlib
import subprocess
import urllib.request
import urllib.error
from pathlib import Path

REPO = "dedicate617/godot472"
DIST_DIR = Path(r"Z:\D\work\OpenSource\mindscada-engine-ext\dist")

def get_github_token() -> str:
    # 1. Environment variable
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        return token
    # 2. Git credential manager
    try:
        p = subprocess.Popen(['git', 'credential', 'fill'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out, _ = p.communicate(b'protocol=https\nhost=github.com\n\n')
        for line in out.decode('utf-8', errors='ignore').splitlines():
            if line.startswith('password='):
                return line[9:].strip()
    except Exception as e:
        print(f"[!] Failed to get token from git credential: {e}")
    return ""

def get_latest_successful_run(token: str) -> dict:
    url = f"https://api.github.com/repos/{REPO}/actions/runs?status=completed&conclusion=success&per_page=10"
    headers = {'User-Agent': 'Mozilla/5.0'}
    if token:
        headers['Authorization'] = f"Bearer {token}"
    
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req) as resp:
        data = json.loads(resp.read().decode())
        runs = data.get('workflow_runs', [])
        if not runs:
            raise RuntimeError("No successful workflow runs found!")
        # Return the latest one
        return runs[0]

def get_run_artifacts(run_id: int, token: str) -> list:
    url = f"https://api.github.com/repos/{REPO}/actions/runs/{run_id}/artifacts"
    headers = {'User-Agent': 'Mozilla/5.0'}
    if token:
        headers['Authorization'] = f"Bearer {token}"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req) as resp:
        data = json.loads(resp.read().decode())
        return data.get('artifacts', [])

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

def download_and_extract_artifact(artifact: dict, token: str, target_dir: Path):
    name = artifact['name']
    art_id = artifact['id']
    size = artifact['size_in_bytes']
    download_url = artifact['archive_download_url']
    
    print(f"\n[*] Processing artifact: {name} (ID: {art_id}, Size: {size / 1024 / 1024:.2f} MB)...")
    
    opener = urllib.request.build_opener(NoRedirect)
    headers = {'User-Agent': 'Mozilla/5.0', 'Authorization': f'Bearer {token}'}
    req = urllib.request.Request(download_url, headers=headers)
    
    redirect_url = None
    try:
        opener.open(req)
    except urllib.error.HTTPError as e:
        if e.code in (301, 302, 303, 307, 308):
            redirect_url = e.headers['Location']
        else:
            raise RuntimeError(f"HTTP error {e.code}: {e.reason}")
            
    if not redirect_url:
        raise RuntimeError("No redirect URL returned for artifact download!")
        
    print(f"    Downloading from blob storage...")
    req2 = urllib.request.Request(redirect_url, headers={'User-Agent': 'Mozilla/5.0'})
    
    temp_zip = target_dir / f".temp_{name}.zip"
    with urllib.request.urlopen(req2) as resp, open(temp_zip, 'wb') as f:
        total = 0
        while True:
            chunk = resp.read(1024 * 1024)
            if not chunk:
                break
            f.write(chunk)
            total += len(chunk)
            print(f"\r    Downloaded {total / 1024 / 1024:.2f} MB / {size / 1024 / 1024:.2f} MB", end="", flush=True)
    print()
    
    print(f"    Extracting to {target_dir}...")
    with zipfile.ZipFile(temp_zip, 'r') as z:
        for member in z.namelist():
            # Don't overwrite existing files with older checksums if not needed
            if member == "checksums.sha256":
                continue
            z.extract(member, target_dir)
            extracted_path = target_dir / member
            print(f"      + {member} ({extracted_path.stat().st_size / 1024 / 1024:.2f} MB)")
            
    # Cleanup temp zip
    if temp_zip.exists():
        temp_zip.unlink()

def regenerate_checksums(target_dir: Path):
    manifest_path = target_dir / "checksums.sha256"
    entries = []
    for item in sorted(target_dir.iterdir()):
        if item.is_file() and item.name != "checksums.sha256" and not item.name.startswith("."):
            hasher = hashlib.sha256()
            with open(item, "rb") as f:
                while chunk := f.read(1024 * 1024):
                    hasher.update(chunk)
            digest = hasher.hexdigest()
            entries.append(f"{digest}  {item.name}")
            
    with open(manifest_path, "w", encoding="utf-8") as f:
        f.write("\n".join(entries) + "\n")
    print(f"\n[+] Regenerated unified checksums manifest: {manifest_path} ({len(entries)} files)")

def main():
    print("==================================================================")
    print(" MindSCADA Engine - Download Online Latest CI/CD Assets to dist/  ")
    print("==================================================================")
    
    token = get_github_token()
    if not token:
        print("[-] Error: Unable to obtain GitHub token from environment or Git Credential Manager.")
        sys.exit(1)
    print(f"[+] GitHub token obtained successfully.")
    
    DIST_DIR.mkdir(parents=True, exist_ok=True)
    
    run = get_latest_successful_run(token)
    run_id = run['id']
    print(f"[+] Latest Successful Workflow Run: ID={run_id}")
    print(f"    Name:       {run['name']}")
    print(f"    Branch:     {run['head_branch']}")
    print(f"    Commit SHA: {run['head_sha']}")
    print(f"    Commit Msg: {run.get('head_commit', {}).get('message', '').splitlines()[0]}")
    print(f"    Created:    {run['created_at']}")
    print(f"    Updated:    {run['updated_at']}")
    
    artifacts = get_run_artifacts(run_id, token)
    print(f"\n[+] Found {len(artifacts)} build artifacts:")
    for a in artifacts:
        print(f"    - {a['name']:25s} ({a['size_in_bytes'] / 1024 / 1024:6.2f} MB)")
        
    for a in artifacts:
        download_and_extract_artifact(a, token, DIST_DIR)
        
    regenerate_checksums(DIST_DIR)
    
    print("\n==================================================================")
    print(f"[SUCCESS] All CI/CD assets downloaded and extracted to {DIST_DIR}!")
    print("==================================================================")
    for item in sorted(DIST_DIR.iterdir()):
        if item.is_file():
            print(f"  {item.stat().st_size:10d} bytes  {item.name}")

if __name__ == '__main__':
    main()
