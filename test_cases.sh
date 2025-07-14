#!/bin/bash

echo "=== Testing Step 1: Integer Constants ==="

# Test case 1: Basic case
echo "Test 1: Basic integer return"
echo "int main() { return 42; }" > test1.c
./wacc test1.c && echo "✅ Compiled successfully" || echo "❌ Compilation failed"

# Test case 2: Different integer
echo "Test 2: Different integer"
echo "int main() { return 123; }" > test2.c
./wacc test2.c && echo "✅ Compiled successfully" || echo "❌ Compilation failed"

# Test case 3: Zero
echo "Test 3: Return zero"
echo "int main() { return 0; }" > test3.c
./wacc test3.c && echo "✅ Compiled successfully" || echo "❌ Compilation failed"

# Test case 4: Different function name (should fail for now)
echo "Test 4: Different function name"
echo "int foo() { return 42; }" > test4.c
./wacc test4.c && echo "✅ Compiled successfully" || echo "❌ Compilation failed (expected)"

echo "=== Test Results ==="
cat out.wat
