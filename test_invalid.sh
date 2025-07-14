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
        if ./wacc "$test_file" > /dev/null 2>&1; then
            echo "FAIL - should have failed to compile"
            FAILED=$((FAILED + 1))
        else
            echo "PASS"
            PASSED=$((PASSED + 1))
            
            # TODO: Parse expected error information from comments
            # and validate that the correct errors are reported
        fi
    fi
done

echo ""
echo "Invalid tests summary: $PASSED passed, $FAILED failed, $TOTAL total"
exit $FAILED
