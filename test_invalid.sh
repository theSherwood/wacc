#!/bin/bash

# Test script for invalid compile-time tests
# These tests should fail to parse and include expected error information

echo "Running invalid compile-time tests..."

PASSED=0
FAILED=0
TOTAL=0

for test_file in tests/invalid/*.c; do
    if [ -f "$test_file" ]; then
        TOTAL=$((TOTAL + 1))
        echo -n "Testing $(basename "$test_file"): "
        
        # Try to compile the test file - should fail
        output=$(./wacc "$test_file" 2>&1)
        exit_code=$?
        
        if [ $exit_code -eq 0 ]; then
            echo "FAIL - should have failed to compile"
            FAILED=$((FAILED + 1))
        else
            # Parse expected error information from comments
            expected_line=$(grep "^// ERROR:" "$test_file" | head -1)
            if [ -n "$expected_line" ]; then
                # Extract line number and error ID from comment
                line_num=$(echo "$expected_line" | sed 's/.*Line \([0-9]*\).*/\1/')
                error_id=$(echo "$expected_line" | sed 's/.*Error \([0-9]*\).*/\1/')

                # Check if compiler output contains expected error
                if echo "$output" | grep -q "$error_id" && echo "$output" | grep -q ":$line_num:"; then
                    echo "PASS"
                    PASSED=$((PASSED + 1))
                else
                    echo "FAIL - expected error $error_id at line $line_num"
                    echo "  Got: $output"
                    FAILED=$((FAILED + 1))
                fi
            else
                echo "PASS"
                PASSED=$((PASSED + 1))
            fi
        fi
    fi
done

echo ""
echo "Invalid tests summary: $PASSED passed, $FAILED failed, $TOTAL total"
exit $FAILED
