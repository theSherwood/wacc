int main() {
    int x = 0;
    while (x < 10) {
        x = x + 1;
        if (x == 5) {
            break;
        }
    }
    return x;
}

// Expected: 5
