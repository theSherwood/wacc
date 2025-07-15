#!/bin/bash

# Main test runner script
# Runs all test suites: valid, invalid, and runtime tests

echo "=========================================="
echo "Running wacc compiler test suite"
echo "=========================================="

# Build the compiler first
echo "Building compiler..."
if ! make > /dev/null 2>&1; then
    echo "ERROR: Failed to build compiler"
    exit 1
fi

# Make test scripts executable
chmod +x scripts/test_valid.sh scripts/test_invalid.sh scripts/test_runtime.sh

# Run all test suites
TOTAL_FAILED=0

echo ""
./scripts/test_valid.sh
TOTAL_FAILED=$((TOTAL_FAILED + $?))

echo ""
./scripts/test_invalid.sh
TOTAL_FAILED=$((TOTAL_FAILED + $?))

echo ""
./scripts/test_runtime.sh
TOTAL_FAILED=$((TOTAL_FAILED + $?))

echo ""
echo "=========================================="
if [ $TOTAL_FAILED -eq 0 ]; then
    echo "All tests passed!"
else
    echo "Some tests failed (total failures: $TOTAL_FAILED)"
fi
echo "=========================================="

exit $TOTAL_FAILED
