#!/bin/bash

# Test script for runtime tests
# These tests should be compiled and run, with output verified

echo "Running runtime tests..."

PASSED=0
FAILED=0
TOTAL=0

for test_file in tests/runtime/*.c; do
    if [ -f "$test_file" ]; then
        TOTAL=$((TOTAL + 1))
        echo -n "Testing $(basename "$test_file"): "
        
        # Compile the test file
        if ./wacc "$test_file" > /dev/null 2>&1; then
            # Check if WASM file was generated
            if [ -f "out.wasm" ]; then
                # Run the WASM file using Node.js
                if command -v node > /dev/null 2>&1; then
                    # Extract expected output from comments in the test file
                    expected_output=$(grep -o "// Expected: [0-9]*" "$test_file" | sed 's/\/\/ Expected: //')
                    
                    if [ -n "$expected_output" ]; then
                        actual_output=$(node test_wasm.js 2>/dev/null)
                        if [ "$actual_output" = "$expected_output" ]; then
                            echo "PASS"
                            PASSED=$((PASSED + 1))
                        else
                            echo "FAIL - expected $expected_output, got $actual_output"
                            FAILED=$((FAILED + 1))
                        fi
                    else
                        echo "SKIP - no expected output specified"
                    fi
                else
                    echo "SKIP - Node.js not available"
                fi
            else
                echo "FAIL - WASM file not generated"
                FAILED=$((FAILED + 1))
            fi
        else
            echo "FAIL - compilation failed"
            FAILED=$((FAILED + 1))
        fi
    fi
done

echo ""
echo "Runtime tests summary: $PASSED passed, $FAILED failed, $TOTAL total"
exit $FAILED
