#!/bin/bash
# Build-only VPN development variant; kept separate from the extension image.
set -euo pipefail

if [ "$#" -ne 3 ]; then
	printf 'Usage: %s WORKDIR MERLIN_CHECKOUT NEW_SOURCE_DIRECTORY\n' "$0" >&2
	exit 1
fi
script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
port_dir=$(cd -- "$1" && pwd)
merlin_dir=$(cd -- "$2" && pwd)
source_dir=$3
if [ -e "$source_dir" ] || [ -L "$source_dir" ]; then
	printf 'Source directory already exists; left unchanged: %s\n' "$source_dir" >&2
	exit 1
fi
for tool in python3 git patch sha256sum; do
	command -v "$tool" >/dev/null || { printf '%s not found\n' "$tool" >&2; exit 1; }
done
revision=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["revision"])' \
	"$script_dir/vpn/imports.json")
git -C "$merlin_dir" cat-file -e "$revision^{commit}"
mkdir -p -- "$(dirname -- "$source_dir")"
stage=$(mktemp -d "${source_dir}.XXXXXX")
trap 'rm -rf -- "$stage"' EXIT
bash "$script_dir/prepare-port.sh" "$port_dir" "$stage/source"

# Read pinned Git objects, independent of the checkout's branch or local edits.
python3 - "$script_dir/vpn/imports.json" "$merlin_dir" "$stage/source/asuswrt" <<'PY'
import json
import io
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile

manifest = json.load(open(sys.argv[1]))
root = Path(sys.argv[3])
for name in manifest.get("trees", []):
    # Replace complete packages so obsolete vendor sources cannot survive.
    if not name.startswith("release/src/router/") or ".." in Path(name).parts:
        raise SystemExit("Invalid tree import path: " + name)
    data = subprocess.check_output([
        "git", "-C", sys.argv[2], "archive", manifest["revision"], name,
    ])
    archive = tarfile.open(fileobj=io.BytesIO(data))
    members = archive.getmembers()
    for member in members:
        if member.isdir():
            continue
        if (not member.isfile() or not member.name.startswith(name + "/")
                or ".." in Path(member.name).parts):
            raise SystemExit("Invalid tree member: " + member.name)
    target = root / name
    if target.is_symlink():
        raise SystemExit("Unexpected package symlink: " + name)
    if target.exists():
        shutil.rmtree(target)
    for member in members:
        if member.isfile():
            path = root / member.name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(archive.extractfile(member).read())
            path.chmod(member.mode & 0o777)
    archive.close()
for destination, source in manifest["files"].items():
    for name in (destination, source):
        if not name.startswith("release/src/router/") or ".." in Path(name).parts:
            raise SystemExit("Invalid import path: " + name)
    data = subprocess.check_output([
        "git", "-C", sys.argv[2], "show", manifest["revision"] + ":" + source,
    ])
    path = root / destination
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
PY
patch --directory="$stage/source/asuswrt" -p1 --fuzz=0 --batch --forward \
	< "$script_dir/vpn/0001-vpn-integration.patch"
printf '%s\n' "$revision" > "$stage/source/merlin-source.rev"
(cd "$script_dir/vpn" && sha256sum imports.json source-files.json 0001-vpn-integration.patch) \
	>> "$stage/source/port-patches.sha256"
python3 - "$script_dir/vpn/source-files.json" "$stage/source" "$script_dir/vpn/imports.json" <<'PY'
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[2])
names = set(json.load(open(sys.argv[1])))
for tree in json.load(open(sys.argv[3])).get("trees", []):
    names.update(str(path.relative_to(root / "asuswrt"))
                 for path in (root / "asuswrt" / tree).rglob("*") if path.is_file())
with (root / "vpn-source-files.sha256").open("w") as output:
    for name in sorted(names):
        digest = hashlib.sha256((root / "asuswrt" / name).read_bytes()).hexdigest()
        output.write(digest + "  " + name + "\n")
PY
mv -T -- "$stage/source" "$source_dir"
printf 'VPN development source: %s/asuswrt\n' "$source_dir"
printf '%s\n' 'Configuration migration and hardware validation remain pending.'
