#!/usr/bin/env python3
"""Package a release under one immutable build path; no network or dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--raw", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--build-id", required=True)
parser.add_argument("--firmware", type=Path)
a = parser.parse_args()
if not re.fullmatch(r"[0-9a-f]{40}", a.build_id):
    parser.error("build-id must be the full Git commit")
if a.output.resolve() in (a.source.resolve(), a.raw.resolve()):
    parser.error("output must be separate from source and raw build")
release = a.output / "assets" / a.build_id
release.mkdir(parents=True, exist_ok=True)
for name in ("app.js", "style.css"):
    shutil.copyfile(a.source / name, release / name)
for name in ("gravelbyte.js", "gravelbyte.wasm"):
    shutil.copyfile(a.raw / name, release / name)
html = (a.source / "index.html").read_text()
html = html.replace('content="BUILD_ID"', f'content="{a.build_id}"')
for name in ("app.js", "style.css", "gravelbyte.uf2"):
    html = html.replace(f'"{name}"', f'"assets/{a.build_id}/{name}"')
(a.output / "index.html").write_text(html)
for name in ("sitemap.xml",):
    if (a.source / name).exists(): shutil.copyfile(a.source / name, a.output / name)
(a.output / ".nojekyll").touch()
(a.output / "version.json").write_text(json.dumps({"commit": a.build_id}) + "\n")
if a.firmware:
    shutil.copyfile(a.firmware, release / "gravelbyte.uf2")
    # Stable download URL remains available to existing README/bookmarks.
    shutil.copyfile(a.firmware, a.output / "gravelbyte.uf2")
    sums = []
    for name in ("gravelbyte.uf2", "gravelbyte.wasm"):
        digest = hashlib.sha256((release / name).read_bytes()).hexdigest()
        sums.append(f"{digest}  assets/{a.build_id}/{name}")
    (a.output / "SHA256SUMS").write_text("\n".join(sums) + "\n")
