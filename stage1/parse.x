int TK_NUM    = 0;
int TK_STR    = 1;
int TK_ID     = 2;
int TK_INT    = 3;
int TK_IF     = 4;
int TK_ELSE   = 5;
int TK_WHILE  = 6;
int TK_RETURN = 7;
int TK_PLUS   = 8;
int TK_MINUS  = 9;
int TK_STAR   = 10;
int TK_SLASH  = 11;
int TK_PCT    = 12;
int TK_EQ     = 13;
int TK_NEQ    = 14;
int TK_LT     = 15;
int TK_LE     = 16;
int TK_GT     = 17;
int TK_GE     = 18;
int TK_AND    = 19;
int TK_OR     = 20;
int TK_BANG   = 21;
int TK_ASSIGN = 22;
int TK_AMP    = 23;
int TK_LPAREN = 24;
int TK_RPAREN = 25;
int TK_LBRACE = 26;
int TK_RBRACE = 27;
int TK_LBRACK = 28;
int TK_RBRACK = 29;
int TK_SEMI   = 30;
int TK_COMMA  = 31;
int TK_EOF    = 32;

int TOK_TYPE = 0;
int TOK_NUM  = 4;
int TOK_STR  = 8;
int TOK_LEN  = 12;
int TOK_LINE = 16;
int TOK_SZ   = 20;

int isspace(int c) {
	if (c == 32)  { return 1; }
	if (c == 9)   { return 1; }
	if (c == 10)  { return 1; }
	if (c == 13)  { return 1; }
	return 0;
}

int isdigit(int c) {
	if (c >= 48) {
		if (c <= 57) { return 1; }
	}
	return 0;
}

int isalpha(int c) {
	if (c >= 65) {
		if (c <= 90)  { return 1; }
	}
	if (c >= 97) {
		if (c <= 122) { return 1; }
	}
	if (c == 95) { return 1; }
	return 0;
}

int isalnum(int c) {
	if (isalpha(c)) { return 1; }
	if (isdigit(c)) { return 1; }
	return 0;
}

int streq(int a, int b) {
	int i = 0;
	int done = 0;
	int res = 0;
	int ca = 0;
	int cb = 0;
	while (done == 0) {
		ca = bload(a, i);
		cb = bload(b, i);
		if (ca != cb) { res = 0; done = 1; }
		else {
			if (ca == 0) { res = 1; done = 1; }
			else { i = i + 1; }
		}
	}
	return res;
}

int streqn(int a, int b, int n) {
	int i = 0;
	int res = 0;
	int done = 0;
	int ca = 0;
	int cb = 0;
	while (done == 0) {
		if (i == n) {
			if (bload(b, i) == 0) { res = 1; } else { res = 0; }
			done = 1;
		} else {
			ca = bload(a, i);
			cb = bload(b, i);
			if (ca != cb) { res = 0; done = 1; }
			else { i = i + 1; }
		}
	}
	return res;
}
int kwint    = "int";
int kwif     = "if";
int kwelse   = "else";
int kwwhile  = "while";
int kwreturn = "return";

int mktok(int type, int num, int str, int len, int line) {
	int t = balloc(TOK_SZ);
	int *p = t;
	p[0] = type;
	p[1] = num;
	p[2] = str;
	p[3] = len;
	p[4] = line;
	return t;
}

int lx_one(int src, int *pos, int *line) {
	int done = 0;
	int c = 0;
	int cc = 0;
	int ln = 0;
	int start = 0;
	int len = 0;
	int sc = 0;
	int num = 0;
	int id = 0;
	int type = 0;
	while (done == 0) {
		c = bload(src, *pos);
		while (isspace(c)) {
			if (c == 10) { *line = *line + 1; }
			*pos = *pos + 1;
			c = bload(src, *pos);
		}
		if (c == 35) {
			cc = bload(src, *pos);
			while (cc != 0) {
				if (cc == 10) {
					*pos = *pos + 1;
					cc = 0;
				} else {
					*pos = *pos + 1;
					cc = bload(src, *pos);
				}
			}
		} else {
			done = 1;
		}
	}
	ln = *line;
	if (c == 0) { return mktok(TK_EOF, 0, 0, 0, ln); }
	if (c == 34) {
		*pos = *pos + 1;
		start = *pos;
		len = 0;
		sc = bload(src, *pos);
		while (sc != 0) {
			if (sc != 34) {
				if (sc == 92) { *pos = *pos + 1; }
				*pos = *pos + 1;
				len = len + 1;
				sc = bload(src, *pos);
			} else {
				sc = 0;
			}
		}
		*pos = *pos + 1;
		return mktok(TK_STR, 0, src + start, len, ln);
	}
	if (isdigit(c)) {
		num = 0;
		while (isdigit(bload(src, *pos))) {
			num = num * 10 + bload(src, *pos) - 48;
			*pos = *pos + 1;
		}
		return mktok(TK_NUM, num, 0, 0, ln);
	}
	if (isalpha(c)) {
		start = *pos;
		while (isalnum(bload(src, *pos))) { *pos = *pos + 1; }
		len = *pos - start;
		id = src + start;
		type = TK_ID;
		if (streqn(id, kwint, len))    { type = TK_INT; }
		if (streqn(id, kwif, len))     { type = TK_IF; }
		if (streqn(id, kwelse, len))   { type = TK_ELSE; }
		if (streqn(id, kwwhile, len))  { type = TK_WHILE; }
		if (streqn(id, kwreturn, len)) { type = TK_RETURN; }
		return mktok(type, 0, id, len, ln);
	}
	*pos = *pos + 1;
	if (c == 43) { return mktok(TK_PLUS,   0, 0, 0, ln); }
	if (c == 45) { return mktok(TK_MINUS,  0, 0, 0, ln); }
	if (c == 42) { return mktok(TK_STAR,   0, 0, 0, ln); }
	if (c == 47) { return mktok(TK_SLASH,  0, 0, 0, ln); }
	if (c == 37) { return mktok(TK_PCT,    0, 0, 0, ln); }
	if (c == 40) { return mktok(TK_LPAREN, 0, 0, 0, ln); }
	if (c == 41) { return mktok(TK_RPAREN, 0, 0, 0, ln); }
	if (c == 123){ return mktok(TK_LBRACE, 0, 0, 0, ln); }
	if (c == 125){ return mktok(TK_RBRACE, 0, 0, 0, ln); }
	if (c == 91) { return mktok(TK_LBRACK, 0, 0, 0, ln); }
	if (c == 93) { return mktok(TK_RBRACK, 0, 0, 0, ln); }
	if (c == 59) { return mktok(TK_SEMI,   0, 0, 0, ln); }
	if (c == 44) { return mktok(TK_COMMA,  0, 0, 0, ln); }
	if (c == 38) {
		if (bload(src, *pos) == 38) { *pos = *pos + 1; return mktok(TK_AND, 0, 0, 0, ln); }
		return mktok(TK_AMP, 0, 0, 0, ln);
	}
	if (c == 124) {
		if (bload(src, *pos) == 124) { *pos = *pos + 1; return mktok(TK_OR, 0, 0, 0, ln); }
	}
	if (c == 33) {
		if (bload(src, *pos) == 61) { *pos = *pos + 1; return mktok(TK_NEQ, 0, 0, 0, ln); }
		return mktok(TK_BANG, 0, 0, 0, ln);
	}
	if (c == 61) {
		if (bload(src, *pos) == 61) { *pos = *pos + 1; return mktok(TK_EQ, 0, 0, 0, ln); }
		return mktok(TK_ASSIGN, 0, 0, 0, ln);
	}
	if (c == 60) {
		if (bload(src, *pos) == 61) { *pos = *pos + 1; return mktok(TK_LE, 0, 0, 0, ln); }
		return mktok(TK_LT, 0, 0, 0, ln);
	}
	if (c == 62) {
		if (bload(src, *pos) == 61) { *pos = *pos + 1; return mktok(TK_GE, 0, 0, 0, ln); }
		return mktok(TK_GT, 0, 0, 0, ln);
	}
	return mktok(TK_EOF, 0, 0, 0, ln);
}

int tok_type(int t) { int *p = t; return p[0]; }
int tok_num(int t)  { int *p = t; return p[1]; }
int tok_str(int t)  { int *p = t; return p[2]; }
int tok_len(int t)  { int *p = t; return p[3]; }
int tok_line(int t) { int *p = t; return p[4]; }

int LX_SRC  = 0;
int LX_POS  = 4;
int LX_LINE = 8;
int LX_CUR  = 12;
int LX_PEEK = 16;
int LX_SZ   = 20;

int lx_new(int src) {
	int l = balloc(LX_SZ);
	int *p = l;
	p[0] = src;
	p[1] = 0;
	p[2] = 1;
	p[3] = 0;
	p[4] = 0;
	return l;
}

int lx_src(int l)      { int *p = l; return p[0]; }
int lx_pos_ref(int l)  { return l + LX_POS; }
int lx_line_ref(int l) { return l + LX_LINE; }
int lx_cur(int l)      { int *p = l; return p[3]; }
int lx_peek(int l)     { int *p = l; return p[4]; }
int lx_set_cur(int l, int t)  { int *p = l; p[3] = t; return 0; }
int lx_set_peek(int l, int t) { int *p = l; p[4] = t; return 0; }

int lx_advance(int l) {
	int *p = l;
	int pos_ref = lx_pos_ref(l);
	int line_ref = lx_line_ref(l);
	p[3] = p[4];
	p[4] = lx_one(p[0], pos_ref, line_ref);
	return 0;
}

int lx_init(int l) {
	int pos_ref = lx_pos_ref(l);
	int line_ref = lx_line_ref(l);
	int *p = l;
	p[4] = lx_one(p[0], pos_ref, line_ref);
	lx_advance(l);
	return 0;
}

int lx_cur_type(int l)  { return tok_type(lx_cur(l)); }
int lx_cur_num(int l)   { return tok_num(lx_cur(l)); }
int lx_cur_str(int l)   { return tok_str(lx_cur(l)); }
int lx_cur_len(int l)   { return tok_len(lx_cur(l)); }

int lcheck(int l, int t) { return lx_cur_type(l) == t; }

int lmatch(int l, int t) {
	if (lcheck(l, t)) { lx_advance(l); return 1; }
	return 0;
}

int lexpect(int l, int t) {
	int tok = lx_cur(l);
	if (lcheck(l, t) == 0) {
		putstr("parse error: unexpected token\n");
		putint(lx_cur_type(l));
		putstr(" expected ");
		putint(t);
		putstr("\n");
	}
	lx_advance(l);
	return tok;
}

int ND_NUM        = 0;
int ND_STR        = 1;
int ND_ID         = 2;
int ND_DECL       = 3;
int ND_ASSIGN     = 4;
int ND_BIN        = 5;
int ND_UN         = 6;
int ND_DEREF      = 7;
int ND_ADDR       = 8;
int ND_INDEX      = 9;
int ND_DEREF_ASSIGN = 10;
int ND_BLOCK      = 11;
int ND_IF         = 12;
int ND_WHILE      = 13;
int ND_FN         = 14;
int ND_CALL       = 15;
int ND_RETURN     = 16;
int ND_PROG       = 17;

int ND_SZ   = 232;
int MAX_CH  = 16;
int MAX_PAR = 16;

int nd_new(int t) {
	int n = balloc(ND_SZ);
	int *p = n;
	p[0] = t;
	return n;
}

int nd_type(int n) { int *p = n; return p[0]; }
int nd_num(int n)  { int *p = n; return p[1]; }
int nd_str(int n)  { int *p = n; return p[2]; }
int nd_slen(int n) { int *p = n; return p[3]; }
int nd_ptr(int n)  { int *p = n; return p[4]; }
int nd_a(int n)    { int *p = n; return p[5]; }
int nd_b(int n)    { int *p = n; return p[6]; }
int nd_c(int n)    { int *p = n; return p[7]; }
int nd_nch(int n)  { int *p = n; return p[8]; }
int nd_ch(int n, int i) { int *p = n; return p[9 + i]; }
int nd_npar(int n) { int *p = n; return p[25]; }
int nd_par_str(int n, int i) { int *p = n; return p[26 + i]; }
int nd_par_ptr(int n, int i) { int *p = n; return p[42 + i]; }

int nd_set_num(int n, int v)  { int *p = n; p[1] = v; return 0; }
int nd_set_str(int n, int v)  { int *p = n; p[2] = v; return 0; }
int nd_set_slen(int n, int v) { int *p = n; p[3] = v; return 0; }
int nd_set_ptr(int n, int v)  { int *p = n; p[4] = v; return 0; }
int nd_set_a(int n, int v)    { int *p = n; p[5] = v; return 0; }
int nd_set_b(int n, int v)    { int *p = n; p[6] = v; return 0; }
int nd_set_c(int n, int v)    { int *p = n; p[7] = v; return 0; }

int nd_add_ch(int n, int ch) {
	int *p = n;
	int i = p[8];
	p[9 + i] = ch;
	p[8] = i + 1;
	return 0;
}

int nd_add_par(int n, int s, int isptr) {
	int *p = n;
	int i = p[25];
	p[26 + i] = s;
	p[42 + i] = isptr;
	p[25] = i + 1;
	return 0;
}


int pprim(int l) {
	int t = lx_cur(l);
	int tt = tok_type(t);
	int n = 0;
	if (tt == TK_NUM) {
		lx_advance(l);
		n = nd_new(ND_NUM);
		nd_set_num(n, tok_num(t));
		return n;
	}
	if (tt == TK_STR) {
		lx_advance(l);
		n = nd_new(ND_STR);
		nd_set_str(n, tok_str(t));
		nd_set_slen(n, tok_len(t));
		return n;
	}
	if (tt == TK_ID) {
		lx_advance(l);
		if (lcheck(l, TK_LPAREN)) {
			lx_advance(l);
			n = nd_new(ND_CALL);
			nd_set_str(n, tok_str(t));
			if (lcheck(l, TK_RPAREN) == 0) {
				nd_add_ch(n, pexpr(l));
				while (lmatch(l, TK_COMMA)) {
					nd_add_ch(n, pexpr(l));
				}
			}
			lexpect(l, TK_RPAREN);
			return n;
		}
		n = nd_new(ND_ID);
		nd_set_str(n, tok_str(t));
		return n;
	}
	if (tt == TK_LPAREN) {
		lx_advance(l);
		n = pexpr(l);
		lexpect(l, TK_RPAREN);
		return n;
	}
	putstr("pprim: unexpected token ");
	putint(tt);
	putstr("\n");
	return 0;
}

int punary(int l) {
	int n = 0;
	int base = 0;
	int idx = 0;
	int wrap = 0;
	if (lcheck(l, TK_BANG)) {
		lx_advance(l);
		n = nd_new(ND_UN);
		nd_set_num(n, TK_BANG);
		nd_set_a(n, punary(l));
		return n;
	}
	if (lcheck(l, TK_MINUS)) {
		lx_advance(l);
		n = nd_new(ND_UN);
		nd_set_num(n, TK_MINUS);
		nd_set_a(n, punary(l));
		return n;
	}
	if (lcheck(l, TK_STAR)) {
		lx_advance(l);
		n = nd_new(ND_DEREF);
		nd_set_a(n, punary(l));
		return n;
	}
	if (lcheck(l, TK_AMP)) {
		lx_advance(l);
		n = nd_new(ND_ADDR);
		nd_set_a(n, punary(l));
		return n;
	}
	base = pprim(l);
	while (lcheck(l, TK_LBRACK)) {
		lx_advance(l);
		idx = pexpr(l);
		lexpect(l, TK_RBRACK);
		wrap = nd_new(ND_INDEX);
		nd_set_a(wrap, base);
		nd_set_b(wrap, idx);
		base = wrap;
	}
	return base;
}

int pmul(int l) {
	int n = punary(l);
	int op = 0;
	int r = 0;
	while (lcheck(l, TK_STAR) || lcheck(l, TK_SLASH) || lcheck(l, TK_PCT)) {
		op = lx_cur_type(l);
		lx_advance(l);
		r = nd_new(ND_BIN);
		nd_set_num(r, op);
		nd_set_a(r, n);
		nd_set_b(r, punary(l));
		n = r;
	}
	return n;
}

int padd(int l) {
	int n = pmul(l);
	int op = 0;
	int r = 0;
	while (lcheck(l, TK_PLUS) || lcheck(l, TK_MINUS)) {
		op = lx_cur_type(l);
		lx_advance(l);
		r = nd_new(ND_BIN);
		nd_set_num(r, op);
		nd_set_a(r, n);
		nd_set_b(r, pmul(l));
		n = r;
	}
	return n;
}

int pcmp(int l) {
	int n = padd(l);
	int op = 0;
	int r = 0;
	while (lcheck(l, TK_LT) || lcheck(l, TK_LE) || lcheck(l, TK_GT) || lcheck(l, TK_GE)) {
		op = lx_cur_type(l);
		lx_advance(l);
		r = nd_new(ND_BIN);
		nd_set_num(r, op);
		nd_set_a(r, n);
		nd_set_b(r, padd(l));
		n = r;
	}
	return n;
}

int peq(int l) {
	int n = pcmp(l);
	int op = 0;
	int r = 0;
	while (lcheck(l, TK_EQ) || lcheck(l, TK_NEQ)) {
		op = lx_cur_type(l);
		lx_advance(l);
		r = nd_new(ND_BIN);
		nd_set_num(r, op);
		nd_set_a(r, n);
		nd_set_b(r, pcmp(l));
		n = r;
	}
	return n;
}

int pand(int l) {
	int n = peq(l);
	int r = 0;
	while (lcheck(l, TK_AND)) {
		lx_advance(l);
		r = nd_new(ND_BIN);
		nd_set_num(r, TK_AND);
		nd_set_a(r, n);
		nd_set_b(r, peq(l));
		n = r;
	}
	return n;
}

int por(int l) {
	int n = pand(l);
	int r = 0;
	while (lcheck(l, TK_OR)) {
		lx_advance(l);
		r = nd_new(ND_BIN);
		nd_set_num(r, TK_OR);
		nd_set_a(r, n);
		nd_set_b(r, pand(l));
		n = r;
	}
	return n;
}

int pexpr(int l) {
	int n = por(l);
	int r = 0;
	int nt = nd_type(n);
	if (lcheck(l, TK_ASSIGN)) {
		lx_advance(l);
		if (nt == ND_ID) {
			r = nd_new(ND_ASSIGN);
			nd_set_str(r, nd_str(n));
			nd_set_a(r, pexpr(l));
			return r;
		}
		if (nt == ND_DEREF || nt == ND_INDEX) {
			r = nd_new(ND_DEREF_ASSIGN);
			nd_set_a(r, n);
			nd_set_b(r, pexpr(l));
			return r;
		}
		putstr("pexpr: invalid assignment target\n");
		return 0;
	}
	return n;
}

int pblock(int l) {
	int n = 0;
	lexpect(l, TK_LBRACE);
	n = nd_new(ND_BLOCK);
	while (lcheck(l, TK_RBRACE) == 0 && lcheck(l, TK_EOF) == 0) {
		nd_add_ch(n, pstmt(l));
	}
	lexpect(l, TK_RBRACE);
	return n;
}

int pstmt(int l) {
	int tt = lx_cur_type(l);
	int t = 0;
	int isptr = 0;
	int nm = 0;
	int n = 0;
	if (tt == TK_INT) {
		lx_advance(l);
		isptr = 0;
		if (lcheck(l, TK_STAR)) { lx_advance(l); isptr = 1; }
		nm = lexpect(l, TK_ID);
		lexpect(l, TK_ASSIGN);
		n = nd_new(ND_DECL);
		nd_set_str(n, tok_str(nm));
		nd_set_ptr(n, isptr);
		nd_set_a(n, pexpr(l));
		lexpect(l, TK_SEMI);
		return n;
	}
	if (tt == TK_RETURN) {
		lx_advance(l);
		n = nd_new(ND_RETURN);
		if (lcheck(l, TK_SEMI) == 0) { nd_set_a(n, pexpr(l)); }
		lexpect(l, TK_SEMI);
		return n;
	}
	if (tt == TK_IF) {
		lx_advance(l);
		n = nd_new(ND_IF);
		lmatch(l, TK_LPAREN);
		nd_set_a(n, pexpr(l));
		lmatch(l, TK_RPAREN);
		nd_set_b(n, pblock(l));
		if (lmatch(l, TK_ELSE)) {
			if (lcheck(l, TK_IF)) {
				nd_set_c(n, pstmt(l));
			} else {
				nd_set_c(n, pblock(l));
			}
		}
		return n;
	}
	if (tt == TK_WHILE) {
		lx_advance(l);
		n = nd_new(ND_WHILE);
		lmatch(l, TK_LPAREN);
		nd_set_a(n, pexpr(l));
		lmatch(l, TK_RPAREN);
		nd_set_b(n, pblock(l));
		return n;
	}
	if (tt == TK_LBRACE) {
		return pblock(l);
	}
	n = pexpr(l);
	lexpect(l, TK_SEMI);
	return n;
}

int ptoplevel(int l) {
	int tt = lx_cur_type(l);
	int isptr = 0;
	int nm = 0;
	int pp = 0;
	int pname = 0;
	int n = 0;
	if (tt == TK_INT) {
		lx_advance(l);
		isptr = 0;
		if (lcheck(l, TK_STAR)) { lx_advance(l); isptr = 1; }
		nm = lexpect(l, TK_ID);
		if (lcheck(l, TK_LPAREN)) {
			lx_advance(l);
			n = nd_new(ND_FN);
			nd_set_str(n, tok_str(nm));
			nd_set_ptr(n, isptr);
			if (lcheck(l, TK_RPAREN) == 0) {
				lexpect(l, TK_INT);
				pp = 0;
				if (lcheck(l, TK_STAR)) { lx_advance(l); pp = 1; }
				pname = lexpect(l, TK_ID);
				nd_add_par(n, tok_str(pname), pp);
				while (lmatch(l, TK_COMMA)) {
					lexpect(l, TK_INT);
					pp = 0;
					if (lcheck(l, TK_STAR)) { lx_advance(l); pp = 1; }
					pname = lexpect(l, TK_ID);
					nd_add_par(n, tok_str(pname), pp);
				}
			}
			lexpect(l, TK_RPAREN);
			nd_set_a(n, pblock(l));
			return n;
		}
		lexpect(l, TK_ASSIGN);
		n = nd_new(ND_DECL);
		nd_set_str(n, tok_str(nm));
		nd_set_ptr(n, isptr);
		nd_set_a(n, pexpr(l));
		lexpect(l, TK_SEMI);
		return n;
	}
	return pstmt(l);
}

int parse(int src) {
	int l = lx_new(src);
	int prog = 0;
	int n = 0;
	lx_init(l);
	prog = nd_new(ND_PROG);
	while (lcheck(l, TK_EOF) == 0) {
		nd_add_ch(prog, ptoplevel(l));
	}
	return prog;
}

int nd_type_name(int t) {
	if (t == ND_NUM)          { return "NUM"; }
	if (t == ND_STR)          { return "STR"; }
	if (t == ND_ID)           { return "ID"; }
	if (t == ND_DECL)         { return "DECL"; }
	if (t == ND_ASSIGN)       { return "ASSIGN"; }
	if (t == ND_BIN)          { return "BIN"; }
	if (t == ND_UN)           { return "UN"; }
	if (t == ND_DEREF)        { return "DEREF"; }
	if (t == ND_ADDR)         { return "ADDR"; }
	if (t == ND_INDEX)        { return "INDEX"; }
	if (t == ND_DEREF_ASSIGN) { return "DEREF_ASSIGN"; }
	if (t == ND_BLOCK)        { return "BLOCK"; }
	if (t == ND_IF)           { return "IF"; }
	if (t == ND_WHILE)        { return "WHILE"; }
	if (t == ND_FN)           { return "FN"; }
	if (t == ND_CALL)         { return "CALL"; }
	if (t == ND_RETURN)       { return "RETURN"; }
	if (t == ND_PROG)         { return "PROG"; }
	return "?";
}

int dump(int n, int depth) {
	int i = 0;
	int d = 0;
	int ch = 0;
	int t = 0;
	if (n == 0) { return 0; }
	d = 0;
	while (d < depth) { putstr("  "); d = d + 1; }
	t = nd_type(n);
	putstr(nd_type_name(t));
	if (t == ND_NUM)  { putstr(" "); putint(nd_num(n)); }
	if (t == ND_ID)   { putstr(" "); putstr(nd_str(n)); }
	if (t == ND_DECL) { putstr(" "); putstr(nd_str(n)); }
	if (t == ND_FN)   { putstr(" "); putstr(nd_str(n)); }
	if (t == ND_CALL) { putstr(" "); putstr(nd_str(n)); }
	if (t == ND_ASSIGN) { putstr(" "); putstr(nd_str(n)); }
	putstr("\n");
	dump(nd_a(n), depth + 1);
	dump(nd_b(n), depth + 1);
	dump(nd_c(n), depth + 1);
	i = 0;
	while (i < nd_nch(n)) {
		ch = nd_ch(n, i);
		dump(ch, depth + 1);
		i = i + 1;
	}
	return 0;
}

int src = "int fact(int n) { if (n == 0) { return 1; } return n * fact(n - 1); }\nputint(fact(10));\nputstr(\"\n\");\n";
int prog = parse(src);
putstr("!!! AST !!!\n");
dump(prog, 0);
