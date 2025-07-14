#!/bin/bash

# Test script for valid compile-time tests
# These tests should parse successfully

echo "Running valid compile-time tests..."

PASSED=0
FAILED=0
TOTAL=0

for test_file in tests/valid/*.c; do
    if [ -f "$test_file" ]; then
        TOTAL=$((TOTAL + 1))
        echo -n "Testing $(basename "$test_file"): "
        
        # Try to compile the test file
        if ./wacc "$test_file" > /dev/null 2>&1; then
            echo "PASS"
            PASSED=$((PASSED + 1))
        else
            echo "FAIL - should have compiled successfully"
            FAILED=$((FAILED + 1))
        fi
    fi
done

echo ""
echo "Valid tests summary: $PASSED passed, $FAILED failed, $TOTAL total"
exit $FAILED
