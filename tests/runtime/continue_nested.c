int main() {
  int x = 0;
  while (x < 10) {
    x = x + 1;
    if (1) {
      if (1) {
        continue;
      }
    }
    x = 42;
  }
  return x;
}

// Expected: 10
