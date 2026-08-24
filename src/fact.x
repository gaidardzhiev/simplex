int fact(int n) {
	if (n == 0) { return 1; }
	return n * fact(n - 1);
}

putint(fact(10));
putstr("\n");
