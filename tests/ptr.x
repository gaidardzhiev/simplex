int g = 7;
int h = 9;
int arr0 = 0;
int arr1 = 0;
int arr2 = 0;
int arr3 = 0;
int f(int *p, int i) {
	int v = p[i];
	p[i] = v * 2;
	return p[i];
}
int g2(int a) {
	int l = a;
	int *q = &l;
	*q = *q + 100;
	int *r = &a;
	*r = *r + 1;
	return l * 1000 + a;
}
int *pg = &g;
putint(*pg); putstr("\n");
*pg = 11;
putint(g); putstr("\n");
int *pa = &arr0;
pa[0] = 1; pa[1] = 2; pa[2] = 3; pa[3] = 4;
putint(f(pa, 2)); putstr("\n");
putint(pa[2]); putint(arr2); putstr("\n");
putint(g2(5)); putstr("\n");
int pp = &pa;
int *ppp = pp;
putint(**ppp == 1);
putint(*(pa + 4)); putint(*(pa + 8));
putstr("\n");
pa[1 + 1] = pa[0 + 3] + 40;
*(pa + 12) = 77;
putint(arr2); putint(arr3);
putstr("\n");
int z = h = g = 3;
putint(z); putint(h); putint(g);
putstr("\n");
