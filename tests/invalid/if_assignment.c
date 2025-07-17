int main() {
    int flag = 0;
    int a = if (flag)
                2;
            else
                3;
    return a;
}
// ERROR: Line 3, Error 2009
// ERROR: Line 5, Error 2008
