int c0() { return 42; }
int c1(int a) { return a + 1; }
int c2(int a, int b) { return a * 10 + b; }
int c3(int a, int b, int c) { return a * 100 + b * 10 + c; }
int c4(int a, int b, int c, int d) { return c3(a, b, c) * 10 + d; }
int c5(int a, int b, int c, int d, int e) { return c4(a, b, c, d) * 10 + e; }
int c6(int a, int b, int c, int d, int e, int f) { return c5(a, b, c, d, e) * 10 + f; }
int c7(int a, int b, int c, int d, int e, int f, int g) { return c6(a, b, c, d, e, f) % 1000 * 10 + g; }
int c8(int a, int b, int c, int d, int e, int f, int g, int h) { return a + b + c + d + e + f + g + h; }
int even(int n) { if (n == 0) { return 1; } return odd(n - 1); }
int odd(int n) { if (n == 0) { return 0; } return even(n - 1); }
int fib(int n) { if (n < 2) { return n; } return fib(n - 1) + fib(n - 2); }
int noret() { putint(5); }
int bare() { return; }
putint(c0()); putstr(" ");
putint(c1(c1(c1(0)))); putstr(" ");
putint(c2(1, 2)); putstr(" ");
putint(c3(1, 2, 3)); putstr(" ");
putint(c4(1, 2, 3, 4)); putstr(" ");
putint(c5(1, 2, 3, 4, 5)); putstr(" ");
putint(c6(1, 2, 3, 4, 5, 6)); putstr(" ");
putint(c7(1, 2, 3, 4, 5, 6, 7)); putstr(" ");
putint(c8(1, 2, 3, 4, 5, 6, 7, 8)); putstr("\n");
putint(even(10)); putint(odd(7)); putint(even(7)); putint(fib(15)); putstr("\n");
putint(c2(c1(1), c2(c1(2), c1(3)))); putstr("\n");
putint(bare()); noret(); putstr("\n");
