#!/bin/bash

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUTPUT_DIR="$PROJECT_ROOT/output"

# Build type: release or debug
PROTEUS_BUILD_TYPE="${PROTEUS_BUILD_TYPE:-release}"

if [ "$PROTEUS_BUILD_TYPE" = "debug" ]; then
    ASAN_PATH=$(gcc -print-file-name=libasan.so)
    if [ ! -f "$ASAN_PATH" ]; then
        echo "Warning: libasan.so not found at $ASAN_PATH"
        echo "Make sure AddressSanitizer is installed:"
        echo "  sudo apt-get install libasan"
        LD_PRELOAD_VALUE="./libproteus_hook.so"
    else
        LD_PRELOAD_VALUE="$ASAN_PATH ./libproteus_hook.so"
    fi
else
    LD_PRELOAD_VALUE="./libproteus_hook.so"
fi

cd "$OUTPUT_DIR"

test_results=()
test_names=()

run_test() {
    local test_name="$1"
    local test_binary="$2"
    shift 2
    
    echo "=========================================================="
    echo "Test $test_name"
    echo "=========================================================="
    
    if [ ! -f "./$test_binary" ]; then
        echo "❌ SKIPPED: $test_binary not found!"
        echo "=========================================================="
        test_results+=(1)
        test_names+=("$test_name")
        return 1
    fi    

    LD_PRELOAD="$LD_PRELOAD_VALUE" "./$test_binary" "$@"
    local result=$?
    
    test_results+=($result)
    test_names+=("$test_name")
    
    if [ $result -eq 0 ]; then
        echo "✅ Test $test_name PASSED (return code: $result)"
    else
        echo "❌ Test $test_name FAILED (return code: $result)"
    fi
    echo "=========================================================="
    
    return $result
}

run_test "i2c_client_test" "i2c_client_test" "/dev/i2c-1" "0x51"
run_test "i2c_eeprom_client_test" "i2c_eeprom_client_test" "/dev/i2c-2" "0x52"
run_test "i2c_smbus_client_test" "i2c_smbus_client_test" "/dev/i2c-3" "0x53"
run_test "i2c_multi_client_test" "i2c_multi_client_test"

run_test "spi_client_test" "spi_client_test" "/dev/spidev0.0"
run_test "spi_multi_client_test" "spi_multi_client_test"


GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

failed=0

max_len=0
for name in "${test_names[@]}"; do
    len=${#name}
    [ $len -gt $max_len ] && max_len=$len
done
max_len=$((max_len + 2))

echo ""
echo "=========================================================="
echo "TEST SUMMARY"
echo "=========================================================="

for i in "${!test_results[@]}"; do
    if [ ${test_results[$i]} -eq 0 ]; then
        printf "%-${max_len}s ${GREEN}✓ PASSED${NC}\n" "${test_names[$i]}"
    else
        printf "%-${max_len}s ${RED}✗ FAILED${NC}\n" "${test_names[$i]}"
        failed=1
    fi
done
echo "=========================================================="

if [ $failed -eq 0 ]; then
    echo "All tests passed! 🎉"
    exit 0
else
    echo "Some tests failed! ⚠️"
    exit 1
fi