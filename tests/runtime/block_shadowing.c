int main() {
    int x = 10;
    {
        int x = 20;
        return x;
    }
}

// Expected: 20
