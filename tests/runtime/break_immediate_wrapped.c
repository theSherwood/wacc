int main() {
  int x = 0;
  if (1) {
    while (1) {
      x = 42;
      break;
    }
    x = 3;
  }
  return x;
}

// Expected: 3
