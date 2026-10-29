int classify(int n) {
	if (n == 0) { return 100; }
	else if (n < 10) { return 200; }
	else if (n < 100) { return 300; }
	else { return 400; }
	return 999;
}
int count(int n) {
	int i = 0;
	int j = 0;
	int t = 0;
	while (i < n) {
		j = 0;
		while (j < i) {
			if (j % 2 == 0) { t = t + 1; } else { t = t + 10; }
			j = j + 1;
		}
		i = i + 1;
	}
	return t;
}
int early(int n) {
	int i = 0;
	while (1) {
		if (i == n) { return i * 3; }
		i = i + 1;
	}
	return 0;
}
int noelse(int n) {
	int r = 1;
	if (n) { r = 2; }
	if (n == 0) { r = r + 10; }
	{ r = r + 100; }
	return r;
}
putint(classify(0)); putint(classify(5)); putint(classify(50)); putint(classify(500)); putstr("\n");
putint(count(7)); putstr("\n");
putint(early(9)); putstr("\n");
putint(noelse(0)); putint(noelse(3)); putstr("\n");
if (1) { putstr("top-level if\n"); } else { putstr("never\n"); }
int k = 0;
while (k < 3) { putint(k); k = k + 1; }
putstr("\n");
