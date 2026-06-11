#!/usr/bin/env bash
set -euo pipefail

url="${1:-http://localhost:5175/}"
browser="${AST_BROWSER:-google-chrome}"
profile="${AST_BROWSER_PROFILE:-/tmp/asm-shader-toy-webgpu-profile}"

exec "$browser" \
  --user-data-dir="$profile" \
  --enable-unsafe-webgpu \
  --enable-features=Vulkan \
  --ignore-gpu-blocklist \
  --new-window \
  "$url"
