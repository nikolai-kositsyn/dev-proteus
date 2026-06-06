#!/bin/bash

# ========== Default configuration ==========
# Cross-compilation prefix (empty by default)
# Examples:
#   - aarch64-linux-gnu-    (for ARM64)
#   - arm-linux-gnueabihf-  (for ARM32)
#   - x86_64-linux-gnu-     (for x86_64)
CROSS_COMPILE="${CROSS_COMPILE:-}"

# ========== Build clients ==========
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLIENTS_DIR="$PROJECT_ROOT/tests"
OUTPUT_DIR="$PROJECT_ROOT/output"

cd "$CLIENTS_DIR" || exit 1

# Show build configuration
echo "Building test clients..."
if [ -n "$CROSS_COMPILE" ]; then
    echo "  Cross-compilation: $CROSS_COMPILE"
else
    echo "  Native compilation (no cross-compilation)"
fi
echo ""

# Clean and build with cross-compilation if specified
if [ -n "$CROSS_COMPILE" ]; then
    make clean
    make CROSS_COMPILE="$CROSS_COMPILE"
else
    make clean && make
fi

# Check build result
if [ $? -ne 0 ]; then
    echo "❌ Client build failed!"
    exit 1
fi

# ========== Copy results ==========
mkdir -p "$OUTPUT_DIR"

# Copy clients if they exist
for client in i2c_client i2c_eeprom_client i2c_smbus_client; do
    if [ -f "$CLIENTS_DIR/$client" ]; then
        cp "$CLIENTS_DIR/$client" "$OUTPUT_DIR/"
        echo "  Copied: $client"
    else
        echo "  Warning: $client not found"
    fi
done

echo ""
echo "✅ Clients build successful!"
echo "Clients copied to: $OUTPUT_DIR/"