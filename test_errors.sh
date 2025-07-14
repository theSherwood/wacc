#!/bin/bash

echo "=== Testing Error Cases ==="

# Test case 1: Missing semicolon
echo "Test 1: Missing semicolon"
echo "int main() { return 42 }" > error1.c
./wacc error1.c && echo "❌ Should have failed" || echo "✅ Correctly failed"

# Test case 2: Missing return statement
echo "Test 2: Missing return statement"
echo "int main() { }" > error2.c
./wacc error2.c && echo "❌ Should have failed" || echo "✅ Correctly failed"

# Test case 3: Missing braces
echo "Test 3: Missing braces"
echo "int main() return 42;" > error3.c
./wacc error3.c && echo "❌ Should have failed" || echo "✅ Correctly failed"

# Test case 4: Invalid integer
echo "Test 4: Invalid integer"
echo "int main() { return abc; }" > error4.c
./wacc error4.c && echo "❌ Should have failed" || echo "✅ Correctly failed"
