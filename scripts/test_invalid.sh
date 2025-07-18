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
        # Parse all expected error information from comments
        expected_errors=$(grep "^// ERROR:" "$test_file")
        if [ -n "$expected_errors" ]; then
            # Extract all line numbers and error IDs from comments
            expected_count=$(echo "$expected_errors" | wc -l)
            all_matched=true
            missing_errors=""
            
            while IFS= read -r expected_line; do
                line_num=$(echo "$expected_line" | sed 's/.*Line \([0-9]*\).*/\1/')
                error_id=$(echo "$expected_line" | sed 's/.*Error \([0-9]*\).*/\1/')
                
                # Check if compiler output contains this specific error
                if ! (echo "$output" | grep -q "$error_id" && echo "$output" | grep -q ":$line_num:"); then
                    all_matched=false
                    missing_errors="$missing_errors error $error_id at line $line_num;"
                fi
            done <<< "$expected_errors"
            
            # Count actual errors reported by compiler
            actual_count=$(echo "$output" | grep -c "error:")
            
            if [ "$all_matched" = true ] && [ "$actual_count" -eq "$expected_count" ]; then
                # echo "$test_name: PASS" > "$tmp_subdir/result.txt"
                # do nothing
                TOTAL=$TOTAL
            else
                echo "----------------------------" > "$tmp_subdir/result.txt"
                echo "$test_name: FAIL - expected $expected_count errors, got $actual_count" >> "$tmp_subdir/result.txt"
                echo "--Expected:" >> "$tmp_subdir/result.txt"
                echo "$expected_errors" >> "$tmp_subdir/result.txt"
                echo "" >> "$tmp_subdir/result.txt"
                if [ "$all_matched" = false ]; then
                    echo "  Missing:$missing_errors" >> "$tmp_subdir/result.txt"
                fi
                echo "--Recieved:" >> "$tmp_subdir/result.txt"
                echo "$output" >> "$tmp_subdir/result.txt"
            fi
        else
            echo "----------------------------" > "$tmp_subdir/result.txt"
            echo "$test_name: FAIL - no expected error information found" >> "$tmp_subdir/result.txt"
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
    if [[ $result == *FAIL* ]]; then
        FAILED=$((FAILED + 1))
    fi
done

PASSED=$((TOTAL - FAILED))

echo ""
echo "Invalid tests summary: $PASSED passed, $FAILED failed, $TOTAL total"

# Clean up
rm -rf "$TMP_DIR"
exit $FAILED
