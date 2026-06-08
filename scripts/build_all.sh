#!/bin/bash

set -e

# ========== Default configuration ==========
PROTEUS_BUILD_TYPE="${PROTEUS_BUILD_TYPE:-release}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=========================================="
echo "Building dev-proteus"
echo "Build type: $PROTEUS_BUILD_TYPE"
echo "=========================================="

# Build hook library
echo ""
echo "[1/2] Building hook library..."
"$SCRIPT_DIR/build_hooks.sh"

# Build test clients
echo ""
echo "[2/2] Building test clients..."
"$SCRIPT_DIR/build_clients.sh"

echo ""
echo "=========================================="
echo "✅ Build complete!"
echo "=========================================="
echo "Output directory: $(cd "$SCRIPT_DIR/.." && pwd)/output"
echo "Build type: $PROTEUS_BUILD_TYPE"

if [ "$PROTEUS_BUILD_TYPE" = "release" ]; then
    echo ""
    echo "To run tests:"
    echo "  ./scripts/run_tests.sh"
else
    echo ""
    echo "To run tests with AddressSanitizer:"
    echo "  ASAN_PATH=\"/usr/lib/gcc/x86_64-linux-gnu/13/libasan.so\""
    echo "  LD_PRELOAD=\"\$ASAN_PATH ./output/libproteus_hook.so\" ./output/i2c_client_test"
fi