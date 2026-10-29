int gs = "global string\n";
int e1 = "tab\there\n";
int e2 = "quote:\" backslash:\\ nul-split:\0hidden\n";
int e3 = "odd escape: \q\z\n";
int f() {
	int ls = "local string\n";
	putstr(ls);
	putstr("direct string\n");
	return ls;
}
putstr(gs); putstr(e1); putstr(e2); putstr(e3);
f();
putint(bload(e2, 30)); putstr(" ");
putint(bload(e2, 31)); putstr(" ");
putint(bload(e1, 3)); putstr("\n");
putstr("");
putstr("same\n"); putstr("same\n");
int gp = "ptr string";
putstr(gp); putstr("\n");
