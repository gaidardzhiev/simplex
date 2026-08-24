int msg = "loop fact(10) = ";
int n = 10;
int result = 1;
while (n > 1) {
	result = result * n;
	n = n - 1;
}
putstr(msg);
putint(result);
putstr("\n");
