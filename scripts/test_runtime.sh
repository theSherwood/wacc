#!/bin/bash

echo "Running runtime tests in parallel..."

TMP_DIR=$(mktemp -d)
JOBS=()
TOTAL=0

run_test() {
    test_file="$1"
    test_name=$(basename "$test_file")        # e.g. add.c
    test_id="${test_name%.c}"
    tmp_subdir="$TMP_DIR/$test_id"
    mkdir -p "$tmp_subdir"

    expected_output=$(grep -o "// Expected: -\?[0-9]*" "$test_file" | sed 's/\/\/ Expected: //')

    # Compile to a unique wasm output file
    ./wacc "$test_file" -o "$tmp_subdir/out.wasm" > /dev/null 2>&1

    if [ -f "$tmp_subdir/out.wasm" ]; then
        if command -v node > /dev/null 2>&1; then
            if [ -n "$expected_output" ]; then
                # Run the WASM file; adjust test_wasm.js to accept a wasm file path or copy it to cwd
                actual_output=$(node test_wasm.js "$tmp_subdir/out.wasm" 2>/dev/null)
                if [ "$actual_output" = "$expected_output" ]; then
                    # echo "$test_name: PASS" > "$tmp_subdir/result.txt"
                    # do nothing
                    TOTAL=$TOTAL
                else
                    echo "$test_name: FAIL - expected $expected_output, got $actual_output" > "$tmp_subdir/result.txt"
                fi
            else
                echo "$test_name: SKIP - no expected output" > "$tmp_subdir/result.txt"
            fi
        else
            echo "$test_name: SKIP - Node.js not available" > "$tmp_subdir/result.txt"
        fi
    else
        echo "$test_name: FAIL - WASM file not generated" > "$tmp_subdir/result.txt"
    fi
}

# Run all tests in background
for test_file in tests/runtime/*.c; do
    if [ -f "$test_file" ]; then
        run_test "$test_file" &
        JOBS+=($!)
        TOTAL=$((TOTAL + 1))
    fi
done

# Wait for all background jobs to complete
for pid in "${JOBS[@]}"; do
    wait "$pid"
done

# Aggregate results
PASSED=0
FAILED=0

for result_file in "$TMP_DIR"/*/result.txt; do
    result=$(cat "$result_file")
    echo "$result"
    if [[ $result == *FAIL* ]]; then
        FAILED=$((FAILED + 1))
    fi
done

PASSED=$((TOTAL - FAILED))

echo ""
echo "Runtime tests summary: $PASSED passed, $FAILED failed, $TOTAL total"

# Clean up
rm -rf "$TMP_DIR"
exit $FAILED
