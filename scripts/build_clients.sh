#!/bin/bash

# ========== Default configuration ==========
# Cross-compilation prefix (empty by default)
# Examples:
#   - aarch64-linux-gnu-    (for ARM64)
#   - arm-linux-gnueabihf-  (for ARM32)
#   - x86_64-linux-gnu-     (for x86_64)
CROSS_COMPILE="${CROSS_COMPILE:-}"

# Build type: release or debug
PROTEUS_BUILD_TYPE="${PROTEUS_BUILD_TYPE:-release}"

# ========== Build clients ==========
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLIENTS_DIR="$PROJECT_ROOT/tests"
OUTPUT_DIR="$PROJECT_ROOT/output"

cd "$CLIENTS_DIR" || exit 1

# Show build configuration
echo "Building test clients..."
echo "Build type: $PROTEUS_BUILD_TYPE"
if [ -n "$CROSS_COMPILE" ]; then
    echo "Cross-compilation: $CROSS_COMPILE"
fi
echo ""

# Clean and build
if [ -n "$CROSS_COMPILE" ]; then
    make clean
    make BUILD_TYPE="$PROTEUS_BUILD_TYPE" CROSS_COMPILE="$CROSS_COMPILE"
else
    make clean
    make "$PROTEUS_BUILD_TYPE"
fi

# Check build result
if [ $? -ne 0 ]; then
    echo "❌ Client build failed!"
    exit 1
fi

# ========== Copy results ==========
mkdir -p "$OUTPUT_DIR"

clients=($(ls "$CLIENTS_DIR" | grep -E "^(i2c|spi|uart|gpio).*_test$" | grep -v "\.c$"))

# Copy clients if they exist
for client in "${clients[@]}"; do
    if [ -f "$CLIENTS_DIR/$client" ]; then
        cp "$CLIENTS_DIR/$client" "$OUTPUT_DIR/"
        echo "  Copied: $client"
    else
        echo "  Warning: $client not found"
    fi
done

echo ""
echo "✅ Build successful!"
echo "Clients copied to: $OUTPUT_DIR/"
echo "Build type: $PROTEUS_BUILD_TYPE"

if [ "$PROTEUS_BUILD_TYPE" = "debug" ]; then
    echo ""
    echo "ℹ️  Debug build with AddressSanitizer. Run with:"
    echo "   LD_PRELOAD=\"/usr/lib/gcc/x86_64-linux-gnu/13/libasan.so ./output/libproteus_hook.so\" ./output/i2c_client_test"
fi