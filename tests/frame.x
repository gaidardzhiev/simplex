int f16(int a, int b, int c, int d, int e, int f, int g, int h) {
	int i = 9;
	int j = 10;
	int k = 11;
	int l = 12;
	int m = 13;
	int n = 14;
	int o = 15;
	int p = 16;
	return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o + p;
}
int shadow(int a) {
	int a = a + 1;
	int b = a * 2;
	return a * 100 + b;
}
putint(f16(1, 2, 3, 4, 5, 6, 7, 8)); putstr("\n");
putint(shadow(4)); putstr("\n");
