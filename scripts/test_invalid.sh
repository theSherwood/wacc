#!/bin/bash

# Test script for invalid compile-time tests
# These tests should fail to parse and include expected error information

echo "Running invalid compile-time tests in parallel..."

TMP_DIR=$(mktemp -d)
JOBS=()
TOTAL=0

run_test() {
    test_file="$1"
    test_name=$(basename "$test_file")
    test_id="${test_name%.c}"
    tmp_subdir="$TMP_DIR/$test_id"
    mkdir -p "$tmp_subdir"
    
    # Try to compile the test file - should fail
    output=$(./wacc "$test_file" 2>&1)
    exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo "$test_name: FAIL - should have failed to compile" > "$tmp_subdir/result.txt"
    else
        # Parse expected error information from comments
        expected_line=$(grep "^// ERROR:" "$test_file" | head -1)
        if [ -n "$expected_line" ]; then
            # Extract line number and error ID from comment
            line_num=$(echo "$expected_line" | sed 's/.*Line \([0-9]*\).*/\1/')
            error_id=$(echo "$expected_line" | sed 's/.*Error \([0-9]*\).*/\1/')

            # Check if compiler output contains expected error
            if echo "$output" | grep -q "$error_id" && echo "$output" | grep -q ":$line_num:"; then
                echo "$test_name: PASS" > "$tmp_subdir/result.txt"
            else
                echo "$test_name: FAIL - expected error $error_id at line $line_num" > "$tmp_subdir/result.txt"
                echo "  Got: $output" >> "$tmp_subdir/result.txt"
            fi
        else
            echo "$test_name: PASS" > "$tmp_subdir/result.txt"
        fi
    fi
}

# Run all tests in background
for test_file in tests/invalid/*.c; do
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
    if [[ $result == *PASS ]]; then
        PASSED=$((PASSED + 1))
    elif [[ $result == *FAIL* ]]; then
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "Invalid tests summary: $PASSED passed, $FAILED failed, $TOTAL total"

# Clean up
rm -rf "$TMP_DIR"
exit $FAILED
