int main() {
    int x = 0;
    int y = 0;
    while (x < 5) {
        x = x + 1;
        while (y < 10) {
            y = y + 1;
            if (y == 3) {
                break;
            }
        }
        if (x == 2) {
            break;
        }
    }
    return x + y;
}

// Expected: 5
