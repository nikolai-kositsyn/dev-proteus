#!/bin/bash
# scripts/build_all.sh

echo "=========================================="
echo "Building dev-proteus"
echo "=========================================="

# Build hook library
echo ""
echo "[1/2] Building hook library..."
./scripts/build_hooks.sh
if [ $? -ne 0 ]; then
    echo "❌ Hook library build failed"
    exit 1
fi

# Build test clients
echo ""
echo "[2/2] Building test clients..."
./scripts/build_clients.sh
if [ $? -ne 0 ]; then
    echo "❌ Client build failed"
    exit 1
fi

echo ""
echo "=========================================="
echo "✅ Build complete!"
echo "=========================================="
echo "Output directory: $(cd "$(dirname "$0")/.." && pwd)/output"