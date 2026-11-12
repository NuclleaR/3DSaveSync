#!/usr/bin/env bash
# Builds makerom and bannertool from source, at the versions this project uses,
# and installs them into the given directory. CI runs it in the devkitPro
# container; it works on any Linux with gcc, g++, make, git and zip.
#
#   bash tools/build-cia-tools.sh <install dir>
set -euo pipefail

dest=${1:?usage: build-cia-tools.sh <install dir>}

makerom_tag=makerom-v0.19.0
bannertool_commit=51dac927a4c43de769900d91fd3cb6142295263f

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
jobs=$(nproc)

git clone --quiet --depth 1 --branch "$makerom_tag" https://github.com/3DSGuy/Project_CTR.git "$work/Project_CTR"
make -C "$work/Project_CTR/makerom" -j"$jobs" deps
make -C "$work/Project_CTR/makerom" -j"$jobs"

git clone --quiet https://github.com/Epicpkmn11/bannertool.git "$work/bannertool"
git -C "$work/bannertool" checkout --quiet "$bannertool_commit"
# bannertool's .gitmodules points at git://github.com, which GitHub no longer serves.
git -C "$work/bannertool" -c url."https://github.com/".insteadOf=git://github.com/ submodule update --init --quiet
make -C "$work/bannertool" -j"$jobs"

mkdir -p "$dest"
install -m 755 "$work/Project_CTR/makerom/bin/makerom" "$dest/makerom"
install -m 755 "$work/bannertool/output/linux-x86_64/bannertool" "$dest/bannertool"
