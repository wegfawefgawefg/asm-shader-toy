#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build"
mode="${1:-build}"

configure() {
    cmake --preset dev
}

cd "${repo_root}"

if ! configure; then
    rm -f "${build_dir}/CMakeCache.txt"
    rm -rf "${build_dir}/CMakeFiles"
    configure
fi

if [ "${mode}" = "--configure-only" ]; then
    exit 0
fi

cmake --build --preset dev -j
ctest --test-dir "${build_dir}" --output-on-failure

