#!/bin/bash
# Prepare an isolated stock ASUS build on a Linux x86_64 host.
set -euo pipefail

if [ "$#" -ne 1 ]; then
	printf 'Usage: %s WORKDIR\n' "$0" >&2
	exit 1
fi

if [ "$(uname -sm)" != 'Linux x86_64' ]; then
	printf '%s\n' 'Linux x86_64 required' >&2
	exit 1
fi

for tool in curl sha256sum unzip tar git docker; do
	command -v "$tool" >/dev/null || { printf '%s not found\n' "$tool" >&2; exit 1; }
done

script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
mkdir -p -- "$1"
port_dir=$(cd -- "$1" && pwd)
archive=GPL_TUF_BE9400_300610258138.zip
archive_sha=930142d207b1f9dfc11e2ad17c767abc59a5df5777223dfc99f12a69f1d07b9e
toolchain_rev=5e5b8ae593edb9c3f0711c56ef7fe73e113506d0
toolchain_dir=openwrt-gcc750_musl1124.aarch64

# Kernel and netfilter source names differ only by case.
case_probe=$(mktemp -d "$port_dir/.case-check.XXXXXX")
touch "$case_probe/lower"
if [ -e "$case_probe/LOWER" ]; then
	rm -r -- "$case_probe"
	printf '%s\n' 'Case-sensitive filesystem required' >&2
	exit 1
fi
rm -r -- "$case_probe"

mkdir -p "$port_dir/downloads" "$port_dir/logs"
if [ ! -f "$port_dir/downloads/$archive" ]; then
	curl --fail --location --retry 3 \
		"https://dlcdnets.asus.com/pub/ASUS/wireless/TUF-BE9400/$archive" \
		-o "$port_dir/downloads/$archive.part"
	mv "$port_dir/downloads/$archive.part" "$port_dir/downloads/$archive"
fi
printf '%s  %s\n' "$archive_sha" "$port_dir/downloads/$archive" | sha256sum --check -

if [ ! -e "$port_dir/stock-58138" ]; then
	extract_dir=$(mktemp -d "$port_dir/.stock-58138.XXXXXX")
	unzip -p "$port_dir/downloads/$archive" | tar -xz --no-same-owner -C "$extract_dir"
	mv "$extract_dir" "$port_dir/stock-58138"
fi
test -f "$port_dir/stock-58138/asuswrt/release/src-qca-ipq53xx/platform.mak"

# ASUS shipped LFS pointers; use a pinned candidate toolchain for stock builds.
if [ ! -e "$port_dir/qca-toolchains" ]; then
	git init -q "$port_dir/qca-toolchains"
	git -C "$port_dir/qca-toolchains" remote add origin https://github.com/SWRT-dev/qca-toolchains.git
	git -C "$port_dir/qca-toolchains" config remote.origin.promisor true
	git -C "$port_dir/qca-toolchains" config remote.origin.partialclonefilter blob:none
	git -C "$port_dir/qca-toolchains" sparse-checkout set "$toolchain_dir"
	git -C "$port_dir/qca-toolchains" fetch --depth 1 --filter=blob:none origin "$toolchain_rev"
	git -C "$port_dir/qca-toolchains" checkout --detach FETCH_HEAD
fi
if [ "$(git -C "$port_dir/qca-toolchains" rev-parse HEAD)" != "$toolchain_rev" ]; then
	printf '%s\n' 'Toolchain revision mismatch' >&2
	exit 1
fi
if ! git -C "$port_dir/qca-toolchains" diff --quiet HEAD -- "$toolchain_dir"; then
	printf '%s\n' 'Toolchain has local changes' >&2
	exit 1
fi
if [ -n "$(git -C "$port_dir/qca-toolchains" ls-files --others --exclude-standard -- "$toolchain_dir")" ]; then
	printf '%s\n' 'Untracked toolchain files found' >&2
	exit 1
fi

# Export tracked files only, preserving executable bits and symlinks.
context_dir=$(mktemp -d "$port_dir/.container.XXXXXX")
trap 'rm -rf -- "$context_dir"' EXIT
git -C "$port_dir/qca-toolchains" archive HEAD "$toolchain_dir" |
	tar -x -C "$context_dir"
cp "$script_dir/Dockerfile" "$context_dir/Dockerfile"
docker build -t rt-be90u-build:58138 "$context_dir"
printf '\nStock build environment: %s\n' "$port_dir"
