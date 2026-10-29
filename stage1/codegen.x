int TEXT_SEG  = 4194304;
int HDRSZ     = 232;
int TEXT_BASE = 4194536;
int ROD_BASE  = 5242880;
int BSS_BASE  = 6291456;
int MAX_GLBS  = 1024;
int HEAP_SZ   = 67108864;
int MAX_FNS   = 1024;
int MAX_FPAT  = 32768;
int MAX_LOCS  = 256;
int CODE_MAX  = 4194304;
int ROD_MAX   = 1048576;
int BSS_SZ    = MAX_GLBS * 4 + 36 + HEAP_SZ;
int HEAP_BASE = BSS_BASE + MAX_GLBS * 4 + 32;

int cg_cbuf  = 0;
int cg_clen  = 0;
int cg_rbuf  = 0;
int cg_rlen  = 0;
int cg_gname = 0;
int cg_gstr  = 0;
int cg_gsoff = 0;
int cg_nglb  = 0;
int cg_fname = 0;
int cg_faddr = 0;
int cg_nfn   = 0;
int cg_ppos  = 0;
int cg_pname = 0;
int cg_nfp   = 0;
int cg_lname = 0;
int cg_loff  = 0;
int cg_ln    = 0;
int cg_lfrm  = 0;
int cg_vi    = 0;
int cg_entry = 0;
int cg_tmp   = 0;

int cg_init() {
	cg_cbuf  = balloc(CODE_MAX);
	cg_rbuf  = balloc(ROD_MAX);
	cg_gname = balloc(MAX_GLBS * 4);
	cg_gstr  = balloc(MAX_GLBS * 4);
	cg_gsoff = balloc(MAX_GLBS * 4);
	cg_fname = balloc(MAX_FNS * 4);
	cg_faddr = balloc(MAX_FNS * 4);
	cg_ppos  = balloc(MAX_FPAT * 4);
	cg_pname = balloc(MAX_FPAT * 4);
	cg_lname = balloc(MAX_LOCS * 4);
	cg_loff  = balloc(MAX_LOCS * 4);
	cg_tmp   = balloc(8);
	return 0;
}

int cg_crash() {
	int *z = 0;
	*z = 1;
	return 0;
}

int cg_pnm(int s) {
	int i = 0;
	while (isalnum(bload(s, i))) {
		bstore(bload(s, i), cg_tmp, 0);
		putstr(cg_tmp);
		i = i + 1;
	}
	return 0;
}

int cg_die(int msg) {
	putstr("error: ");
	putstr(msg);
	putstr("\n");
	cg_crash();
	return 0;
}

int cg_die2(int msg, int nm) {
	putstr("error: ");
	putstr(msg);
	putstr(" '");
	cg_pnm(nm);
	putstr("'\n");
	cg_crash();
	return 0;
}

int cg_neq(int a, int b) {
	int i = 0;
	int ca = 0;
	int cb = 0;
	int r = 0;
	int d = 0;
	while (d == 0) {
		ca = bload(a, i);
		cb = bload(b, i);
		if (isalnum(ca) == 0) {
			d = 1;
			if (isalnum(cb) == 0) { r = 1; }
		} else {
			if (ca != cb) { d = 1; } else { i = i + 1; }
		}
	}
	return r;
}

int cg_ae(int w) {
	int *c = cg_cbuf;
	if (cg_clen >= CODE_MAX) { cg_die("code buffer full"); }
	c[cg_clen / 4] = w;
	cg_clen = cg_clen + 4;
	return 0;
}

int a_movi(int rd, int v) {
	int hi = v / 65536;
	cg_ae(3531603968 + v % 65536 * 32 + rd);
	if (hi != 0) { cg_ae(4070572032 + hi * 32 + rd); }
	return 0;
}

int a_mov(int rd, int rm)  { return cg_ae(2852127712 + rm * 65536 + rd); }
int a_add(int rd, int rn, int rm)  { return cg_ae(2332033024 + rm * 65536 + rn * 32 + rd); }
int a_sub(int rd, int rn, int rm)  { return cg_ae(3405774848 + rm * 65536 + rn * 32 + rd); }
int a_addi(int rd, int rn, int im) { return cg_ae(2432696320 + im * 1024 + rn * 32 + rd); }
int a_subi(int rd, int rn, int im) { return cg_ae(3506438144 + im * 1024 + rn * 32 + rd); }
int a_mul(int rd, int rn, int rm)  { return cg_ae(2600500224 + rm * 65536 + rn * 32 + rd); }
int a_udiv(int rd, int rn, int rm) { return cg_ae(2596276224 + rm * 65536 + rn * 32 + rd); }
int a_msub(int rd, int rn, int rm, int ra) { return cg_ae(2600501248 + rm * 65536 + ra * 1024 + rn * 32 + rd); }
int a_cmp(int rn, int rm)  { return cg_ae(3942645791 + rm * 65536 + rn * 32); }
int a_cmpi(int rn, int im) { return cg_ae(4043309087 + im * 1024 + rn * 32); }
int a_cset(int rd, int c)  { return cg_ae(2594113504 + c * 4096 + rd); }
int a_str0(int rt, int rn) { return cg_ae(3103784960 + rn * 32 + rt); }
int a_ldr(int rt, int rn, int off) { return cg_ae(3107979264 + off / 4 * 1024 + rn * 32 + rt); }
int a_stur(int rt, int rn, int d) { return cg_ae(3087007744 + (512 - d % 512) % 512 * 4096 + rn * 32 + rt); }
int a_ldur(int rt, int rn, int d) { return cg_ae(3091202048 + (512 - d % 512) % 512 * 4096 + rn * 32 + rt); }
int a_ldrbr(int rt, int rn, int rm) { return cg_ae(945842176 + rm * 65536 + rn * 32 + rt); }
int a_strbr(int rt, int rn, int rm) { return cg_ae(941647872 + rm * 65536 + rn * 32 + rt); }
int a_strb0(int rt, int rn) { return cg_ae(956301312 + rn * 32 + rt); }
int a_addl2(int rd, int rn, int rm) { return cg_ae(2332035072 + rm * 65536 + rn * 32 + rd); }
int a_neg(int rd, int rm) { return cg_ae(3405775840 + rm * 65536 + rd); }
int a_and(int rd, int rn, int rm) { return cg_ae(2315255808 + rm * 65536 + rn * 32 + rd); }
int a_orr(int rd, int rn, int rm) { return cg_ae(2852126720 + rm * 65536 + rn * 32 + rd); }
int a_svc() { return cg_ae(3556769793); }
int a_ret() { return cg_ae(3596551104); }

int a_push() {
	a_subi(31, 31, 16);
	a_str0(0, 31);
	return 0;
}

int a_pop(int rt) {
	a_ldr(rt, 31, 0);
	a_addi(31, 31, 16);
	return 0;
}

int cg_bph() {
	int p = cg_clen;
	cg_ae(335544320);
	return p;
}

int cg_beq() {
	int p = cg_clen;
	cg_ae(1409286144);
	return p;
}

int cg_bne() {
	int p = cg_clen;
	cg_ae(1409286145);
	return p;
}

int cg_blp(int nm) {
	int *pp = cg_ppos;
	int *pn = cg_pname;
	int i = cg_nfp;
	if (i >= MAX_FPAT) { cg_die("too many call patches"); }
	pp[i] = cg_clen;
	pn[i] = nm;
	cg_nfp = i + 1;
	cg_ae(2483027968);
	return 0;
}

int cg_patch(int pos, int target) {
	int *c = cg_cbuf;
	int w = c[pos / 4];
	int d = target - (TEXT_BASE + pos);
	int off = d / 4;
	if (w / 16777216 == 84) {
		c[pos / 4] = 1409286144 + off % 524288 * 32 + w % 16;
	} else {
		c[pos / 4] = w / 67108864 * 67108864 + off % 67108864;
	}
	return 0;
}

int cg_lfind(int nm) {
	int i = cg_ln;
	int r = 0;
	int *nms = cg_lname;
	int *ofs = cg_loff;
	while (i > 0) {
		i = i - 1;
		if (bload(nms[i], 0) == bload(nm, 0)) {
			if (cg_neq(nms[i], nm)) {
				r = ofs[i];
				i = 0;
			}
		}
	}
	return r;
}

int cg_ladd(int nm, int d) {
	int *nms = cg_lname;
	int *ofs = cg_loff;
	int i = cg_ln;
	if (i >= MAX_LOCS) { cg_die("too many locals"); }
	nms[i] = nm;
	ofs[i] = d;
	cg_ln = i + 1;
	return 0;
}

int cg_gfind(int nm) {
	int i = 0;
	int r = 0;
	int *nms = cg_gname;
	while (i < cg_nglb) {
		if (bload(nms[i], 0) == bload(nm, 0)) {
			if (cg_neq(nms[i], nm)) {
				r = i + 1;
				i = cg_nglb;
			}
		}
		i = i + 1;
	}
	return r;
}

int cg_gadd(int nm, int isstr, int soff) {
	int *nms = cg_gname;
	int *st = cg_gstr;
	int *so = cg_gsoff;
	int i = cg_nglb;
	if (i >= MAX_GLBS) { cg_die("too many globals"); }
	nms[i] = nm;
	st[i] = isstr;
	so[i] = soff;
	cg_nglb = i + 1;
	return i;
}

int cg_freg(int nm) {
	int *fnm = cg_fname;
	int *fa = cg_faddr;
	int i = cg_nfn;
	if (i >= MAX_FNS) { cg_die("too many functions"); }
	fnm[i] = nm;
	fa[i] = TEXT_BASE + cg_clen;
	cg_nfn = i + 1;
	return 0;
}

int cg_rv(int k) {
	if (k == 0) { a_movi(0, cg_vi); }
	if (k == 2) { a_ldur(0, 29, cg_vi); }
	if (k == 3) { a_ldr(0, 1, 0); }
	return 0;
}

int cg_vst(int k) {
	if (k == 2) { a_stur(0, 29, cg_vi); }
	if (k == 3) { a_str0(0, 1); }
	return 0;
}

int cg_vrval(int nm) {
	int lo = cg_lfind(nm);
	int gi = 0;
	int *st = cg_gstr;
	int *so = cg_gsoff;
	if (lo != 0) {
		cg_vi = lo;
		return 2;
	}
	gi = cg_gfind(nm);
	if (gi == 0) { cg_die2("undefined variable", nm); }
	gi = gi - 1;
	if (st[gi] != 0) {
		a_movi(0, ROD_BASE + so[gi]);
		return 1;
	}
	a_movi(1, BSS_BASE + gi * 4);
	return 3;
}

int cg_vlval(int nm) {
	int lo = cg_lfind(nm);
	int gi = 0;
	int *st = cg_gstr;
	if (lo != 0) {
		cg_vi = lo;
		return 2;
	}
	gi = cg_gfind(nm);
	if (gi == 0) { cg_die2("undefined variable", nm); }
	gi = gi - 1;
	if (st[gi] != 0) { cg_die2("assignment to const", nm); }
	a_movi(1, BSS_BASE + gi * 4);
	return 3;
}

int cg_fexit() {
	a_addi(31, 29, 0);
	cg_ae(2831252477);
	a_ret();
	return 0;
}

int cg_fentry(int n) {
	int np = nd_npar(n);
	int i = 0;
	cg_ln = 0;
	cg_ae(2847898621);
	a_addi(29, 31, 0);
	if (np > 0) { a_subi(31, 31, np * 16); }
	while (i < np) {
		a_stur(i, 29, (i + 1) * 16);
		cg_ladd(nd_par_str(n, i), (i + 1) * 16);
		i = i + 1;
	}
	cg_lfrm = np * 16;
	return 0;
}

int cg_gx(int n) {
	return cg_rv(cg_ex(n));
}

int cg_call(int n) {
	int na = nd_nch(n);
	int i = 0;
	if (na > 8) { cg_die("too many arguments (max 8)"); }
	while (i < na) {
		cg_gx(nd_ch(n, i));
		a_push();
		i = i + 1;
	}
	i = na;
	while (i > 0) {
		i = i - 1;
		a_ldr(i, 31, (na - 1 - i) * 16);
	}
	if (na > 0) { a_addi(31, 31, na * 16); }
	cg_blp(nd_str(n));
	return 0;
}

int cg_getc() {
	int sc = BSS_BASE + BSS_SZ - 4;
	a_movi(9, sc);
	a_movi(8, 63);
	a_movi(0, 0);
	a_mov(1, 9);
	a_movi(2, 1);
	a_svc();
	a_movi(9, sc);
	a_ldr(0, 9, 0);
	a_movi(1, 255);
	a_and(0, 0, 1);
	return 1;
}

int cg_bload(int n) {
	cg_gx(nd_ch(n, 0));
	a_push();
	cg_gx(nd_ch(n, 1));
	a_pop(1);
	a_ldrbr(0, 1, 0);
	return 1;
}

int cg_bstore(int n) {
	cg_gx(nd_ch(n, 0));
	a_push();
	cg_gx(nd_ch(n, 1));
	a_push();
	cg_gx(nd_ch(n, 2));
	a_pop(1);
	a_pop(2);
	a_strbr(2, 1, 0);
	return 1;
}

int cg_balloc(int n) {
	cg_gx(nd_ch(n, 0));
	a_mov(9, 0);
	a_movi(1, HEAP_BASE);
	a_ldr(0, 1, 0);
	a_push();
	a_addi(9, 9, 3);
	a_movi(10, 4294967292);
	a_and(9, 9, 10);
	a_add(0, 0, 9);
	a_str0(0, 1);
	a_pop(0);
	return 1;
}

int cg_intr(int n) {
	int nm = nd_str(n);
	int na = nd_nch(n);
	if (cg_neq(nm, "putint")) {
		if (na != 1) { cg_die("putint takes 1 argument"); }
		cg_gx(nd_ch(n, 0));
		cg_blp("__out");
		return 1;
	}
	if (cg_neq(nm, "putstr")) {
		if (na != 1) { cg_die("putstr takes 1 argument"); }
		cg_gx(nd_ch(n, 0));
		cg_blp("__putstr");
		return 1;
	}
	if (cg_neq(nm, "getc")) {
		if (na != 0) { cg_die("getc takes no arguments"); }
		return cg_getc();
	}
	if (cg_neq(nm, "bload")) {
		if (na != 2) { cg_die("bload takes 2 arguments"); }
		return cg_bload(n);
	}
	if (cg_neq(nm, "bstore")) {
		if (na != 3) { cg_die("bstore takes 3 arguments"); }
		return cg_bstore(n);
	}
	if (cg_neq(nm, "balloc")) {
		if (na != 1) { cg_die("balloc takes 1 argument"); }
		return cg_balloc(n);
	}
	return 0;
}

int cg_assign(int n) {
	cg_gx(nd_a(n));
	cg_vst(cg_vlval(nd_str(n)));
	return 1;
}

int cg_un(int n) {
	int op = nd_num(n);
	cg_gx(nd_a(n));
	if (op == TK_MINUS) {
		a_neg(0, 0);
	} else if (op == TK_BANG) {
		a_cmpi(0, 0);
		a_cset(0, 1);
	}
	return 1;
}

int cg_bin(int n) {
	int op = nd_num(n);
	cg_gx(nd_a(n));
	a_push();
	cg_gx(nd_b(n));
	a_pop(1);
	if (op == TK_PLUS) {
		a_add(0, 1, 0);
	} else if (op == TK_MINUS) {
		a_sub(0, 1, 0);
	} else if (op == TK_STAR) {
		a_mul(0, 1, 0);
	} else if (op == TK_SLASH) {
		a_udiv(0, 1, 0);
	} else if (op == TK_PCT) {
		a_udiv(2, 1, 0);
		a_msub(0, 2, 0, 1);
	} else if (op == TK_LT) {
		a_cmp(1, 0);
		a_cset(0, 10);
	} else if (op == TK_LE) {
		a_cmp(1, 0);
		a_cset(0, 12);
	} else if (op == TK_GT) {
		a_cmp(1, 0);
		a_cset(0, 13);
	} else if (op == TK_GE) {
		a_cmp(1, 0);
		a_cset(0, 11);
	} else if (op == TK_EQ) {
		a_cmp(1, 0);
		a_cset(0, 1);
	} else if (op == TK_NEQ) {
		a_cmp(1, 0);
		a_cset(0, 0);
	} else if (op == TK_AND) {
		a_and(0, 1, 0);
	} else if (op == TK_OR) {
		a_orr(0, 1, 0);
	} else {
		cg_die("unknown binop");
	}
	return 1;
}

int cg_callx(int n) {
	if (cg_intr(n) == 0) { cg_call(n); }
	return 1;
}

int cg_deref(int n) {
	cg_gx(nd_a(n));
	a_mov(1, 0);
	a_ldr(0, 1, 0);
	return 1;
}

int cg_addr(int n) {
	int a = nd_a(n);
	int lo = 0;
	int gi = 0;
	if (nd_type(a) != ND_ID) { cg_die("& requires identifier"); }
	lo = cg_lfind(nd_str(a));
	if (lo != 0) {
		a_subi(0, 29, lo);
		return 1;
	}
	gi = cg_gfind(nd_str(a));
	if (gi == 0) { cg_die2("undefined variable", nd_str(a)); }
	a_movi(0, BSS_BASE + (gi - 1) * 4);
	return 1;
}

int cg_index(int n) {
	cg_gx(nd_a(n));
	a_push();
	cg_gx(nd_b(n));
	a_pop(1);
	a_addl2(0, 1, 0);
	a_ldr(0, 0, 0);
	return 1;
}

int cg_dassign(int n) {
	int lhs = nd_a(n);
	int t = nd_type(lhs);
	cg_gx(nd_b(n));
	a_push();
	if (t == ND_DEREF) {
		cg_gx(nd_a(lhs));
		a_mov(1, 0);
	} else if (t == ND_INDEX) {
		cg_gx(nd_a(lhs));
		a_push();
		cg_gx(nd_b(lhs));
		a_pop(1);
		a_addl2(1, 1, 0);
	} else {
		cg_die("deref_assign: bad lhs");
	}
	a_pop(0);
	a_str0(0, 1);
	return 1;
}

int cg_ex(int n) {
	int t = nd_type(n);
	if (t == ND_NUM) {
		cg_vi = nd_num(n);
		return 0;
	}
	if (t == ND_STR) {
		a_movi(0, ROD_BASE + nd_num(n));
		return 1;
	}
	if (t == ND_ID)           { return cg_vrval(nd_str(n)); }
	if (t == ND_ASSIGN)       { return cg_assign(n); }
	if (t == ND_UN)           { return cg_un(n); }
	if (t == ND_BIN)          { return cg_bin(n); }
	if (t == ND_CALL)         { return cg_callx(n); }
	if (t == ND_DEREF)        { return cg_deref(n); }
	if (t == ND_ADDR)         { return cg_addr(n); }
	if (t == ND_INDEX)        { return cg_index(n); }
	if (t == ND_DEREF_ASSIGN) { return cg_dassign(n); }
	cg_die("gen_expr: unhandled node");
	return 1;
}

int cg_blk(int n, int inf) {
	int i = 0;
	int cnt = nd_nch(n);
	while (i < cnt) {
		cg_st(nd_ch(n, i), inf);
		i = i + 1;
	}
	return 0;
}

int cg_decl(int n, int inf) {
	int a = nd_a(n);
	int gi = 0;
	if (inf == 0 && nd_type(a) == ND_STR && nd_ptr(n) == 0) {
		cg_gadd(nd_str(n), 1, nd_num(a));
	} else {
		cg_gx(a);
		if (inf != 0) {
			cg_lfrm = cg_lfrm + 16;
			a_subi(31, 31, 16);
			a_stur(0, 29, cg_lfrm);
			cg_ladd(nd_str(n), cg_lfrm);
		} else {
			gi = cg_gadd(nd_str(n), 0, 0);
			a_movi(1, BSS_BASE + gi * 4);
			a_str0(0, 1);
		}
	}
	return 0;
}

int cg_ret(int n) {
	if (nd_a(n) != 0) {
		cg_gx(nd_a(n));
	} else {
		a_movi(0, 0);
	}
	cg_fexit();
	return 0;
}

int cg_if(int n, int inf) {
	int bf = 0;
	int be = 0;
	int c = nd_c(n);
	cg_gx(nd_a(n));
	a_cmpi(0, 0);
	bf = cg_beq();
	cg_st(nd_b(n), inf);
	if (c != 0) {
		be = cg_bph();
		cg_patch(bf, TEXT_BASE + cg_clen);
		cg_st(c, inf);
		cg_patch(be, TEXT_BASE + cg_clen);
	} else {
		cg_patch(bf, TEXT_BASE + cg_clen);
	}
	return 0;
}

int cg_while(int n, int inf) {
	int top = TEXT_BASE + cg_clen;
	int bf = 0;
	int bb = 0;
	cg_gx(nd_a(n));
	a_cmpi(0, 0);
	bf = cg_beq();
	cg_st(nd_b(n), inf);
	bb = cg_bph();
	cg_patch(bb, top);
	cg_patch(bf, TEXT_BASE + cg_clen);
	return 0;
}

int cg_fn(int n) {
	int skip = cg_bph();
	cg_freg(nd_str(n));
	cg_fentry(n);
	cg_st(nd_a(n), 1);
	a_movi(0, 0);
	cg_fexit();
	cg_patch(skip, TEXT_BASE + cg_clen);
	cg_ln = 0;
	return 0;
}

int cg_st(int n, int inf) {
	int t = nd_type(n);
	if (t == ND_BLOCK)  { return cg_blk(n, inf); }
	if (t == ND_DECL)   { return cg_decl(n, inf); }
	if (t == ND_RETURN) { return cg_ret(n); }
	if (t == ND_IF)     { return cg_if(n, inf); }
	if (t == ND_WHILE)  { return cg_while(n, inf); }
	if (t == ND_FN)     { return cg_fn(n); }
	cg_ex(n);
	return 0;
}

int cg_rodadd(int s, int len) {
	int off = cg_rlen;
	int i = 0;
	int k = 0;
	int c = 0;
	if (off + len + 1 > ROD_MAX) { cg_die("rodata full"); }
	while (k < len) {
		c = bload(s, i);
		if (c == 92) {
			i = i + 1;
			c = bload(s, i);
			if (c == 110) {
				c = 10;
			} else if (c == 116) {
				c = 9;
			} else if (c == 48) {
				c = 0;
			}
		}
		bstore(c, cg_rbuf, off + k);
		i = i + 1;
		k = k + 1;
	}
	bstore(0, cg_rbuf, off + k);
	cg_rlen = off + k + 1;
	return off;
}

int cg_rs(int n) {
	int i = 0;
	int cnt = 0;
	if (n == 0) { return 0; }
	if (nd_type(n) == ND_STR) {
		nd_set_num(n, cg_rodadd(nd_str(n), nd_slen(n)));
		return 0;
	}
	cg_rs(nd_a(n));
	cg_rs(nd_b(n));
	cg_rs(nd_c(n));
	cnt = nd_nch(n);
	while (i < cnt) {
		cg_rs(nd_ch(n, i));
		i = i + 1;
	}
	return 0;
}

int cg_itoa() {
	int sc = BSS_BASE + BSS_SZ - 20;
	int bz = 0;
	int lp = 0;
	int bn = 0;
	int bd = 0;
	cg_freg("__itoa");
	cg_ae(2847822835);
	cg_ae(2835438581);
	a_mov(19, 0);
	a_movi(20, sc + 19);
	a_mov(21, 20);
	a_movi(22, 10);
	a_cmpi(19, 0);
	bz = cg_beq();
	lp = TEXT_BASE + cg_clen;
	a_udiv(23, 19, 22);
	a_msub(24, 23, 22, 19);
	a_addi(24, 24, 48);
	a_strb0(24, 21);
	a_subi(21, 21, 1);
	a_mov(19, 23);
	a_cmpi(19, 0);
	bn = cg_bne();
	cg_patch(bn, lp);
	bd = cg_bph();
	cg_patch(bz, TEXT_BASE + cg_clen);
	a_movi(24, 48);
	a_strb0(24, 21);
	a_subi(21, 21, 1);
	cg_patch(bd, TEXT_BASE + cg_clen);
	a_addi(0, 21, 1);
	a_sub(1, 20, 21);
	cg_ae(2839632885);
	cg_ae(2831307763);
	a_ret();
	return 0;
}

int cg_out() {
	cg_freg("__out");
	cg_ae(2847833085);
	cg_ae(2835436531);
	a_addi(29, 31, 0);
	cg_blp("__itoa");
	a_mov(19, 0);
	a_mov(20, 1);
	a_movi(8, 64);
	a_movi(0, 1);
	a_mov(1, 19);
	a_mov(2, 20);
	a_svc();
	cg_ae(2839630835);
	cg_ae(2831318013);
	a_ret();
	return 0;
}

int cg_putstr() {
	int lp = 0;
	int bz = 0;
	int bk = 0;
	cg_freg("__putstr");
	cg_ae(2847833085);
	cg_ae(2835436531);
	a_addi(29, 31, 0);
	a_mov(19, 0);
	a_movi(20, 0);
	lp = TEXT_BASE + cg_clen;
	a_ldrbr(0, 19, 20);
	a_cmpi(0, 0);
	bz = cg_beq();
	a_addi(20, 20, 1);
	bk = cg_bph();
	cg_patch(bk, lp);
	cg_patch(bz, TEXT_BASE + cg_clen);
	a_movi(8, 64);
	a_movi(0, 1);
	a_mov(1, 19);
	a_mov(2, 20);
	a_svc();
	cg_ae(2839630835);
	cg_ae(2831318013);
	a_ret();
	return 0;
}

int cg_resolve() {
	int i = 0;
	int j = 0;
	int f = 0;
	int *pp = cg_ppos;
	int *pn = cg_pname;
	int *fnm = cg_fname;
	int *fa = cg_faddr;
	while (i < cg_nfp) {
		f = 0;
		j = 0;
		while (j < cg_nfn) {
			if (bload(fnm[j], 0) == bload(pn[i], 0)) {
				if (cg_neq(fnm[j], pn[i])) {
					cg_patch(pp[i], fa[j]);
					f = 1;
					j = cg_nfn;
				}
			}
			j = j + 1;
		}
		if (f == 0) { cg_die2("undefined function", pn[i]); }
		i = i + 1;
	}
	return 0;
}

int cg_gen(int prog) {
	int i = 0;
	int cnt = nd_nch(prog);
	cg_rs(prog);
	cg_itoa();
	cg_out();
	cg_putstr();
	cg_entry = TEXT_BASE + cg_clen;
	cg_ae(2432697321);
	cg_ae(2994536767);
	cg_ae(1409286208);
	cg_ae(3506447359);
	a_movi(0, HEAP_BASE + 4);
	a_movi(1, HEAP_BASE);
	a_str0(0, 1);
	while (i < cnt) {
		cg_st(nd_ch(prog, i), 0);
		i = i + 1;
	}
	a_movi(8, 93);
	a_movi(0, 0);
	a_svc();
	cg_resolve();
	return 0;
}

int cg_w32(int eb, int off, int v) {
	int *p = eb + off;
	*p = v;
	return 0;
}

int cg_ph(int eb, int k, int fl, int off, int va, int fsz, int msz) {
	int b = 64 + 56 * k;
	cg_w32(eb, b, 1);
	cg_w32(eb, b + 4, fl);
	cg_w32(eb, b + 8, off);
	cg_w32(eb, b + 16, va);
	cg_w32(eb, b + 24, va);
	cg_w32(eb, b + 32, fsz);
	cg_w32(eb, b + 40, msz);
	cg_w32(eb, b + 48, 4096);
	return 0;
}

int cg_dump(int eb, int tot) {
	int ob = balloc(65552);
	int k = 0;
	int i = 0;
	int b = 0;
	while (i < tot) {
		b = bload(eb, i);
		bstore(92, ob, k);
		bstore(48 + b / 64, ob, k + 1);
		bstore(48 + b / 8 % 8, ob, k + 2);
		bstore(48 + b % 8, ob, k + 3);
		k = k + 4;
		if (k >= 65536) {
			bstore(0, ob, k);
			putstr(ob);
			k = 0;
		}
		i = i + 1;
	}
	bstore(0, ob, k);
	putstr(ob);
	return 0;
}

int cg_elf() {
	int eb = 0;
	int tot = HDRSZ + cg_clen;
	int tf = tot;
	int ro = 0;
	int j = 0;
	int n = cg_clen / 4;
	int *c = cg_cbuf;
	int *d = 0;
	if (cg_rlen > 0) {
		ro = (tot + 4095) / 4096 * 4096;
		tot = ro + cg_rlen;
		tf = ro;
	}
	eb = balloc(tot + 8);
	d = eb + HDRSZ;
	while (j < n) {
		d[j] = c[j];
		j = j + 1;
	}
	j = 0;
	while (j < cg_rlen) {
		bstore(bload(cg_rbuf, j), eb, ro + j);
		j = j + 1;
	}
	cg_w32(eb, 0, 1179403647);
	cg_w32(eb, 4, 65794);
	cg_w32(eb, 16, 11993090);
	cg_w32(eb, 20, 1);
	cg_w32(eb, 24, cg_entry);
	cg_w32(eb, 32, 64);
	cg_w32(eb, 52, 3670080);
	cg_w32(eb, 56, 4194307);
	cg_ph(eb, 0, 5, 0, TEXT_SEG, tf, tf);
	if (cg_rlen > 0) {
		cg_ph(eb, 1, 4, ro, ROD_BASE, cg_rlen, cg_rlen);
		cg_ph(eb, 2, 6, 0, BSS_BASE, 0, BSS_SZ);
	} else {
		cg_ph(eb, 1, 6, 0, BSS_BASE, 0, BSS_SZ);
	}
	cg_dump(eb, tot);
	return 0;
}
