#!/bin/bash
set -e

# 编译产物放在源码树外，避免污染 SVN 工作副本
BUILD_DIR="$(cd "$(dirname "$0")/.." && pwd)/cbct_client-build"

# Install Qt6 development package if not installed
if ! dpkg -l | grep -q qt6-base-dev; then
    echo "Installing Qt6 development package..."
    sudo apt update
    sudo apt install -y qt6-base-dev
fi

cmake -B "$BUILD_DIR" -S "$(dirname "$0")" -G Ninja
cmake --build "$BUILD_DIR"

echo "Build successful!"
echo "  Core library:  $BUILD_DIR/libcbct_core.so"
echo "UI executable: $BUILD_DIR/cbct_client"
echo "Run with: $BUILD_DIR/cbct_client"
