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

int src = "int x = 42;\nint y = x + 1;\n";
int pos = 0;
int line = 1;
putstr("calling lx_one\n");
putint(kwint);
putstr("\n");
putint(kwif);
putstr("\n");
int tok = lx_one(src, &pos, &line);
while (tok_type(tok) != TK_EOF) {
	putint(tok_type(tok));
	putstr(" ");
	putint(tok_num(tok));
	putstr("\n");
	tok = lx_one(src, &pos, &line);
}
putstr("done\n");
