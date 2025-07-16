int main() {
    int sum = 0;
    int i = 1;
    while (i <= 3) {
        int j = 1;
        while (j <= 2) {
            sum = sum + i * j;
            j = j + 1;
        }
        i = i + 1;
    }
    return sum;
}

// Expected: 18
