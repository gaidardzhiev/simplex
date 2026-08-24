int msg = "hello\n";
int i = 0;
int c = bload(msg, i);
while (c != 0) {
	putint(c);
	putstr("\n");
	i = i + 1;
	c = bload(msg, i);
}
