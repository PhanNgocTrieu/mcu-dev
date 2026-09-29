#!/usr/bin/env bash
# Build the Ubuntu 22.04 Yocto container and run a command inside it.
# With no arguments, open a shell where Poky is already sourced.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "${root}"

mkdir -p "${root}/yocto"
export HOST_UID="$(id -u)"
export HOST_GID="$(id -g)"

docker compose build
exec docker compose run --rm yocto "$@"
