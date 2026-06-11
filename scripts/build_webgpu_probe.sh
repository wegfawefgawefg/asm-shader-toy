#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

cmake -S "${repo_root}" -B "${repo_root}/build-webgpu-probe" \
    -DAST_BUILD_NATIVE_WEBGPU=ON \
    -DAST_BUILD_APP=OFF \
    -DAST_BUILD_TESTS=OFF \
    -DAST_STRICT=ON \
    -DAST_WARN_AS_ERROR=ON

cmake --build "${repo_root}/build-webgpu-probe" --target \
    ast-webgpu-probe \
    ast-webgpu-frame \
    ast-webgpu-surface-probe
