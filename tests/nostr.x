int x = 1;
int f(int n) { if (n < 2) { return n; } return f(n - 1) + f(n - 2); }
putint(f(10));
putint(x);
