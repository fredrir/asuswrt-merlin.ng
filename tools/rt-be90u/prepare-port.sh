#!/bin/bash
# Apply the experimental RT-BE90U port to verified ASUS sources.
set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	printf 'Usage: %s WORKDIR [NEW_SOURCE_DIRECTORY]\n' "$0" >&2
	exit 1
fi
if [ "$(uname -sm)" != 'Linux x86_64' ]; then
	printf '%s\n' 'Linux x86_64 required' >&2
	exit 1
fi
for tool in sha256sum unzip tar patch; do
	command -v "$tool" >/dev/null || { printf '%s not found\n' "$tool" >&2; exit 1; }
done

script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
port_dir=$(cd -- "$1" && pwd)
source_dir=${2:-"$port_dir/experimental-58138"}
archive="$port_dir/downloads/GPL_TUF_BE9400_300610258138.zip"
archive_sha=930142d207b1f9dfc11e2ad17c767abc59a5df5777223dfc99f12a69f1d07b9e
if [ -e "$source_dir" ] || [ -L "$source_dir" ]; then
	printf 'Source directory already exists; left unchanged: %s\n' "$source_dir" >&2
	exit 1
fi
printf '%s  %s\n' "$archive_sha" "$archive" | sha256sum --check -

mkdir -p -- "$(dirname -- "$source_dir")"
extract_dir=$(mktemp -d "${source_dir}.XXXXXX")
trap 'rm -rf -- "$extract_dir"' EXIT
touch "$extract_dir/case-check"
if [ -e "$extract_dir/CASE-CHECK" ]; then
	printf '%s\n' 'Case-sensitive filesystem required' >&2
	exit 1
fi
rm "$extract_dir/case-check"
unzip -p "$archive" | tar -xz --no-same-owner -C "$extract_dir"

while IFS= read -r patch_name; do
	case "$patch_name" in
		''|'#'*) continue ;;
		*/*|*..*) printf 'Invalid patch name: %s\n' "$patch_name" >&2; exit 1 ;;
	esac
	patch --directory="$extract_dir/asuswrt" -p1 --fuzz=0 --batch --forward \
		< "$script_dir/patches/$patch_name"
	(cd "$script_dir/patches" && sha256sum "$patch_name") >> "$extract_dir/port-patches.sha256"
done < "$script_dir/patches/series"
cp "$script_dir/html-enum.py" \
	"$extract_dir/asuswrt/release/src/router/tools/Lnx_ToolHelp/html-enum.py"
(cd "$script_dir" && sha256sum html-enum.py) >> "$extract_dir/port-patches.sha256"
printf '%s\n' "$archive_sha" > "$extract_dir/asus-source.sha256"
mv -T -- "$extract_dir" "$source_dir"
trap - EXIT
printf 'Experimental sources: %s/asuswrt\n' "$source_dir"
