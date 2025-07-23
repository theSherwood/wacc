#!/bin/bash

echo "Running runtime tests sequentially..."

TMP_DIR=$(mktemp -d)
TOTAL=0
PASSED=0
FAILED=0

for test_file in tests/runtime/*.c; do
    if [ ! -f "$test_file" ]; then
        continue
    fi

    test_name=$(basename "$test_file")
    test_id="${test_name%.c}"
    tmp_subdir="$TMP_DIR/$test_id"
    mkdir -p "$tmp_subdir"

    expected_output=$(grep -o "// Expected: -\?[0-9]*" "$test_file" | sed 's/\/\/ Expected: //')
    ./wacc "$test_file" -o "$tmp_subdir/out.wasm" > /dev/null 2>&1

    TOTAL=$((TOTAL + 1))

    if [ -f "$tmp_subdir/out.wasm" ]; then
        if command -v node > /dev/null 2>&1; then
            if [ -n "$expected_output" ]; then
                actual_output=$(node test_wasm.js "$tmp_subdir/out.wasm" 2>/dev/null)
                if [ "$actual_output" = "$expected_output" ]; then
                    echo "$test_name: PASS"
                    PASSED=$((PASSED + 1))
                else
                    echo "$test_name: FAIL - expected $expected_output, got $actual_output"
                    FAILED=$((FAILED + 1))
                fi
            else
                echo "$test_name: SKIP - no expected output"
            fi
        else
            echo "$test_name: SKIP - Node.js not available"
        fi
    else
        echo "$test_name: FAIL - WASM file not generated"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "Runtime tests summary: $PASSED passed, $FAILED failed, $TOTAL total"

# Clean up
rm -rf "$TMP_DIR"
exit $FAILED
