int main() {
    {
        int x = 42;
    }
    return x;  // Error: x is out of scope
}
// ERROR: Line 5, Error 3001