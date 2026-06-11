#!/usr/bin/env bash
set -euo pipefail

url="${1:-http://localhost:5173/}"
browser="${AST_BROWSER:-google-chrome}"
profile="${AST_BROWSER_PROFILE:-$(mktemp -d /tmp/asm-shader-toy-webgpu-profile.XXXXXX)}"

exec "$browser" \
  --user-data-dir="$profile" \
  --no-first-run \
  --no-default-browser-check \
  --enable-unsafe-webgpu \
  --enable-features=Vulkan,VulkanFromANGLE,DefaultANGLEVulkan,UseSkiaRenderer \
  --use-vulkan \
  --use-angle=vulkan \
  --ignore-gpu-blocklist \
  --new-window \
  "$url"
