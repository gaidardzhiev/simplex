int buf = balloc(16);
int n = 0;
int c = getc();
while (c != 0) {
	bstore(c + 1, buf, n);
	n = n + 1;
	c = getc();
}
bstore(0, buf, n);
putstr(buf); putstr("\n");
putint(n); putstr(" "); putint(bload(buf, 0)); putstr("\n");
int p = balloc(1);
int q = balloc(5);
int r = balloc(8);
putint(q - p); putstr(" "); putint(r - q); putstr("\n");
bstore(200, p, 0); bstore(201, p, 1);
putint(bload(p, 0)); putstr(" "); putint(bload(p, 1)); putstr("\n");
