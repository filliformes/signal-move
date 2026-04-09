#!/bin/bash
set -e
MODULE_ID="signal"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."
WIN_ROOT="$(cd "$ROOT" && pwd -W 2>/dev/null || pwd)"

echo "Building $MODULE_ID for ARM64..."
docker build -t schwung-builder "$SCRIPT_DIR"
mkdir -p "$ROOT/dist/$MODULE_ID"

# Create container (docker create + cp pattern — volume mounts fail on Windows)
CONTAINER_ID=$(MSYS_NO_PATHCONV=1 docker create schwung-builder \
    bash -c "mkdir -p /build/dist/$MODULE_ID && \
    dos2unix /build/src/dsp/${MODULE_ID}.c 2>/dev/null; \
    aarch64-linux-gnu-gcc \
        -O2 -shared -fPIC -ffast-math -Wall \
        -o /build/dist/${MODULE_ID}/dsp.so \
        /build/src/dsp/${MODULE_ID}.c \
        -lm && \
    cp /build/src/module.json /build/dist/${MODULE_ID}/ && \
    cd /build/dist && tar -czf ${MODULE_ID}-module.tar.gz ${MODULE_ID}/")

# Copy entire src dir into container
docker cp "$WIN_ROOT/src" "$CONTAINER_ID:/build/src"
docker start -a "$CONTAINER_ID"

# Extract artifacts
docker cp "$CONTAINER_ID:/build/dist/$MODULE_ID/dsp.so" "$WIN_ROOT/dist/$MODULE_ID/"
# Always copy from src (not from inside Docker) so latest edits are captured
cp -f "$ROOT/src/module.json" "$ROOT/dist/$MODULE_ID/module.json"
docker cp "$CONTAINER_ID:/build/dist/$MODULE_ID-module.tar.gz" "$WIN_ROOT/dist/"
docker rm "$CONTAINER_ID" > /dev/null

echo "Built: dist/$MODULE_ID-module.tar.gz"
