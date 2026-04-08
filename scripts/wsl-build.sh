#!/usr/bin/env bash
set -euo pipefail

export DEVKITPRO=/opt/devkitpro
export DEVKITPPC=$DEVKITPRO/devkitPPC
export PATH=$DEVKITPPC/bin:$DEVKITPRO/tools/bin:$PATH

cd "$(dirname "$0")/.."

GAME="${1:-1}"

echo "=== Cleaning ==="
make -f Makefile.wiiu clean || true

echo "=== Building RPX (PACKAGED_GAME=$GAME) ==="
make -f Makefile.wiiu "PACKAGED_GAME=$GAME" -j"$(nproc)" 2>&1

echo "=== Done: bin/RSDKv4.rpx ==="
ls -la bin/RSDKv4.rpx 2>/dev/null || echo "RPX not found!"
