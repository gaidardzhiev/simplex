int SRC_MAX = 4194304;

int mn_read() {
	int buf = balloc(SRC_MAX);
	int n = 0;
	int c = getc();
	while (c != 0) {
		if (n >= SRC_MAX - 1) { cg_die("source too large or missing NUL terminator"); }
		bstore(c, buf, n);
		n = n + 1;
		c = getc();
	}
	bstore(0, buf, n);
	return buf;
}

int mn_run() {
	int src = mn_read();
	int prog = parse(src);
	cg_init();
	cg_gen(prog);
	cg_elf();
	return 0;
}

mn_run();
