int ND_NUM = 0;
int ND_BIN = 1;

int nd_type(int nd) { return nd[0]; }
int nd_num(int nd)  { return nd[1]; }
int nd_ca(int nd)   { return nd[2]; }
int nd_cb(int nd)   { return nd[3]; }

int nd_set_type(int nd, int v) { nd[0] = v; return 0; }
int nd_set_num(int nd, int v)  { nd[1] = v; return 0; }
int nd_set_ca(int nd, int v)   { nd[2] = v; return 0; }
int nd_set_cb(int nd, int v)   { nd[3] = v; return 0; }

int mknum(int val) {
	int nd = balloc(16);
	nd_set_type(nd, ND_NUM);
	nd_set_num(nd, val);
	nd_set_ca(nd, 0);
	nd_set_cb(nd, 0);
	return nd;
}

int mkbin(int a, int b) {
	int nd = balloc(16);
	nd_set_type(nd, ND_BIN);
	nd_set_num(nd, 0);
	nd_set_ca(nd, a);
	nd_set_cb(nd, b);
	return nd;
}

int eval(int nd) {
	if (nd_type(nd) == ND_NUM) { return nd_num(nd); }
	return eval(nd_ca(nd)) + eval(nd_cb(nd));
}

int a = mknum(10);
int b = mknum(5);
int tree = mkbin(a, b);
putint(eval(tree));
putstr("\n");
int c = mknum(3);
int tree2 = mkbin(tree, c);
putint(eval(tree2));
putstr("\n");
