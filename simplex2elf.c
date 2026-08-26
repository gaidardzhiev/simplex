/*
 * Copyright (c) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

/* posix 2008: we want fileno, fseek, the good stuff not the 1970 rations ;) */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "simplex2elf.h"

/* die: print a message and exit no recovery no second chances
 * the kernel will clean up the kernel always cleans up */
static void die(const char *msg) {
	fprintf(stderr, "error: %s\n", msg);
	exit(1);
}

/* dief: die with a format string same philosophy as die(), but with printf
 * because sometimes you want to know which undefined variable killed you */
static void dief(const char *fmt, ...) {
	va_list ap;
	fprintf(stderr, "error: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(1);
}

/* dynamic byte buffer
 * a growable byte array cap doubles on every realloc
 * no linked lists, no pools, no cleverness just realloc and memcpy
 * this is the only data structure you need */

/* zero out a fresh buf d=null means "not yet allocated" */
void buf_init(Buf *b) {
	b->d = NULL;
	b->cap = b->len = 0;
}

/* ensure at least n bytes of capacity doubles until it fits
 * starts at 512 bytes because 1 is a waste of everyone's time */
void buf_reserve(Buf *b, size_t n) {
	if (b->cap >= n) return;
	size_t nc = b->cap ? b->cap * 2 : 512;
	while (nc < n) nc *= 2;
	uint8_t *p = realloc(b->d, nc);
	if (!p) die("realloc");
	b->d = p;
	b->cap = nc;
}

/* append n raw bytes reserve first, then memcpy that's it */
void buf_append(Buf *b, const void *s, size_t n) {
	buf_reserve(b, b->len + n);
	memcpy(b->d + b->len, s, n);
	b->len += n;
}

/* append a single byte used constantly by the code emitter */
void buf_u8(Buf *b, uint8_t x) {
	buf_append(b, &x, 1);
}

/* append a 32-bit value in little-endian byte order
 * aarch64 is le every a64 instruction is 32 bits this gets called a lot */
void buf_u32(Buf *b, uint32_t x) {
	uint8_t tmp[4];
	tmp[0] = x;
	tmp[1] = x >> 8;
	tmp[2] = x >> 16;
	tmp[3] = x >> 24;
	buf_append(b, tmp, 4);
}

/* overwrite 4 bytes at offset off with x (little-endian)
 * used to backpatch branch targets after forward references are resolved */
void buf_patch32(Buf *b, size_t off, uint32_t x) {
	if (off + 4 > b->len) die("patch32 oob");
	b->d[off+0] = x;
	b->d[off+1] = x >> 8;
	b->d[off+2] = x >> 16;
	b->d[off+3] = x >> 24;
}

/* little-endian helpers for elf header construction
 * the elf spec mandates little-endian on aarch64 linux
 * we write the elf header by hand there is no elfh we are the elfh */

/* write a 16-bit le value to an arbitrary byte pointer */
static void le16(uint8_t *p, uint16_t x) {
	p[0]=x; p[1]=x>>8;
}

/* write a 32-bit le value to an arbitrary byte pointer */
static void le32(uint8_t *p, uint32_t x) {
	p[0]=x;
	p[1]=x>>8;
	p[2]=x>>16;
	p[3]=x>>24;
}

/* write a 64-bit le value to an arbitrary byte pointer
 * eight individual byte stores the compiler will figure it out */
static void le64(uint8_t *p, uint64_t x) {
	p[0]=x; p[1]=x>>8;
	p[2]=x>>16; p[3]=x>>24;
	p[4]=x>>32; p[5]=x>>40;
	p[6]=x>>48; p[7]=x>>56;
}

/* character classification
 * we roll our own because <ctypeh> is locale-aware and we do not care
 * about locales simplex identifiers are ascii the universe is ascii */

/* returns 1 if c is a letter or underscore: valid identifier start */
static int isalpha_(char c) {
	return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';
}

/* returns 1 if c is alphanumeric or underscore: valid identifier continuation */
static int isalnum_(char c) {
	return isalpha_(c)||(c>='0'&&c<='9');
}

/* returns 1 if c is a decimal digit */
static int isdigit_(char c) {
	return c>='0'&&c<='9';
}

/* returns 1 if c is whitespace tab, space, newline, carriage return
 * nothing more we are not parsing unicode */
static int isspace_(char c) {
	return c==' '||c=='\t'||c=='\n'||c=='\r';
}

/* lexer
 * the lexer operates on a null-terminated source string
 * it has no file i/o the whole file is loaded into memory before we start
 * one pass, no backtracking, no buffering beyond a one-token lookahead
 * lx_one() consumes one token per call lx_advance() slides the window
 * cur is the current token peek is the next one that's your lookahead */

/* allocate a new lexer state for src caller owns src */
Lx *lx_new(char *src) {
	Lx *l = calloc(1, sizeof(Lx));
	l->src = src;
	l->pos = 0;
	l->line = 1;
	return l;
}

/* zero-initialize a token of type t on the given source line
 * memset because tokens have pointer fields that must start null */
static Tok make_tok(Tk t, int line) {
	Tok tok;
	memset(&tok, 0, sizeof(tok));
	tok.t = t;
	tok.line = line;
	return tok;
}

/* consume and return the next token from the source stream
 * skips whitespace and # line comments
 * handles strings with backslash escapes, integers, identifiers, and
 * all operators unknown characters are a fatal error no recovery */
static Tok lx_one(Lx *l) {
	char *s = l->src;
	while (isspace_(s[l->pos])) {
		if (s[l->pos]=='\n') l->line++;
		l->pos++;
	}
	if (s[l->pos]=='#') {
		while (s[l->pos] && s[l->pos]!='\n') l->pos++;
		return lx_one(l);
	}
	int ln = l->line;
	char c = s[l->pos];
	if (!c) return make_tok(TK_EOF, ln);
	if (c=='"') {
		l->pos++;
		int start = l->pos;
		int len = 0;
		while (s[l->pos] && s[l->pos]!='"') {
			if (s[l->pos]=='\\') l->pos++;
			l->pos++;
			len++;
		}
		if (!s[l->pos]) die("unterminated string");
		char *buf = malloc(len+1);
		int bi = 0, i = start;
		while (s[i] && s[i]!='"') {
			if (s[i]=='\\') {
				i++;
				switch(s[i]) {
				case 'n': buf[bi++]='\n'; break;
				case 't': buf[bi++]='\t'; break;
				case '\\': buf[bi++]='\\'; break;
				case '"': buf[bi++]='"'; break;
				case '0': buf[bi++]=0; break;
				default: buf[bi++]=s[i]; break;
				}
			} else {
				buf[bi++]=s[i];
			}
			i++;
		}
		buf[bi]=0;
		l->pos++;
		Tok tok = make_tok(TK_STR, ln);
		tok.s = buf;
		tok.slen = bi;
		return tok;
	}
	if (isdigit_(c)) {
		long n = 0;
		while (isdigit_(s[l->pos])) n=n*10+(s[l->pos++]-'0');
		Tok tok = make_tok(TK_NUM, ln);
		tok.num = n;
		return tok;
	}
	if (isalpha_(c)) {
		int start = l->pos;
		while (isalnum_(s[l->pos])) l->pos++;
		int len = l->pos - start;
		char *id = malloc(len+1);
		memcpy(id, s+start, len);
		id[len]=0;
		Tk t = TK_ID;
		if (!strcmp(id,"int")) t=TK_INT;
		else if (!strcmp(id,"if")) t=TK_IF;
		else if (!strcmp(id,"else")) t=TK_ELSE;
		else if (!strcmp(id,"while")) t=TK_WHILE;
		else if (!strcmp(id,"return")) t=TK_RETURN;
		Tok tok = make_tok(t, ln);
		tok.s = id;
		return tok;
	}
	l->pos++;
	switch(c) {
	case '+': return make_tok(TK_PLUS, ln);
	case '-': return make_tok(TK_MINUS, ln);
	case '*': return make_tok(TK_STAR, ln);
	case '/': return make_tok(TK_SLASH, ln);
	case '%': return make_tok(TK_PCT, ln);
	case '(': return make_tok(TK_LPAREN, ln);
	case ')': return make_tok(TK_RPAREN, ln);
	case '{': return make_tok(TK_LBRACE, ln);
	case '}': return make_tok(TK_RBRACE, ln);
	case '[': return make_tok(TK_LBRACKET, ln);
	case ']': return make_tok(TK_RBRACKET, ln);
	case ';': return make_tok(TK_SEMI, ln);
	case ',': return make_tok(TK_COMMA, ln);
	case '!':
		if (s[l->pos]=='=') { l->pos++; return make_tok(TK_NEQ, ln); }
		return make_tok(TK_BANG, ln);
	case '=':
		if (s[l->pos]=='=') { l->pos++; return make_tok(TK_EQ, ln); }
		return make_tok(TK_ASSIGN, ln);
	case '<':
		if (s[l->pos]=='=') { l->pos++; return make_tok(TK_LE, ln); }
		return make_tok(TK_LT, ln);
	case '>':
		if (s[l->pos]=='=') { l->pos++; return make_tok(TK_GE, ln); }
		return make_tok(TK_GT, ln);
	case '&':
		if (s[l->pos]=='&') { l->pos++; return make_tok(TK_AND, ln); }
		return make_tok(TK_AMP, ln);
	case '|':
		if (s[l->pos]=='|') { l->pos++; return make_tok(TK_OR, ln); }
		dief("unexpected '|' on line %d", ln);
		break;
	default:
		dief("unexpected char '%c' on line %d", c, ln);
	}
	return make_tok(TK_EOF, ln);
}

/* slide the token window: cur = peek, peek = next token from stream */
void lx_advance(Lx *l) {
	l->cur = l->peek;
	l->peek = lx_one(l);
}

/* prime the two-token window before parsing begins
 * after this: cur is the first real token, peek is the second */
static void lx_init(Lx *l) {
	l->peek = lx_one(l);
	lx_advance(l);
}

/* return the current token without consuming it */
static Tok cur(Lx *l) {
	return l->cur;
}

/* assert that the current token has type t, consume it, and return it
 * if the token is wrong, die with a precise error no "unexpected token" vagueness */
static Tok expect(Lx *l, Tk t) {
	if (l->cur.t != t) dief("expected token %d got %d on line %d", t, l->cur.t, l->cur.line);
	Tok tok = l->cur;
	lx_advance(l);
	return tok;
}

/* return 1 if the current token is of type t, without consuming it */
static int check(Lx *l, Tk t) {
	return l->cur.t == t;
}

/* if the current token is t, consume it and return 1 otherwise return 0
 * used for optional syntax like parentheses around if/while conditions */
static int match(Lx *l, Tk t) {
	if (check(l,t)) {
		lx_advance(l);
		return 1;
	}
	return 0;
}

/* ast node allocation
 * nodes are heap-allocated and never freed during compilation
 * the arena is the process address space the gc is exit() */

/* allocate a zeroed ast node of type t */
Nd *nd_new(Nt t) {
	Nd *n = calloc(1, sizeof(Nd));
	n->t = t;
	return n;
}

/* append child ch to node n's variable-length child array
 * realloc every time could use a doubling strategy does not bother
 * child arrays are short function arg lists are short life is short */
static void nd_add_ch(Nd *n, Nd *ch) {
	n->ch = realloc(n->ch, (n->nch+1)*sizeof(Nd*));
	n->ch[n->nch++] = ch;
}

/* parser
 * recursive descent one function per precedence level
 * the grammar is small enough that the whole thing fits in your head
 * forward declarations for mutual recursion */
static Nd *pexpr(Lx *l);
static Nd *pstmt(Lx *l);
static Nd *pblock(Lx *l);

/* parse a primary expression: number literal, string literal, identifier,
 * function call, or parenthesized sub-expression
 * function calls are recognized by an identifier followed by '(' */
static Nd *pprim(Lx *l) {
	Tok t = cur(l);
	if (t.t == TK_NUM) {
		lx_advance(l);
		Nd *n = nd_new(ND_NUM);
		n->num = t.num;
		return n;
	}
	if (t.t == TK_STR) {
		lx_advance(l);
		Nd *n = nd_new(ND_STR);
		n->s = t.s;
		n->slen = t.slen;
		return n;
	}
	if (t.t == TK_ID) {
		lx_advance(l);
		if (check(l, TK_LPAREN)) {
			lx_advance(l);
			Nd *n = nd_new(ND_CALL);
			n->s = t.s;
			if (!check(l, TK_RPAREN)) {
				do {
					nd_add_ch(n, pexpr(l));
				} while (match(l, TK_COMMA));
			}
			expect(l, TK_RPAREN);
			return n;
		}
		Nd *n = nd_new(ND_ID);
		n->s = t.s;
		return n;
	}
	if (t.t == TK_LPAREN) {
		lx_advance(l);
		Nd *n = pexpr(l);
		expect(l, TK_RPAREN);
		return n;
	}
	dief("unexpected token %d on line %d", t.t, t.line);
	return NULL;
}

/* parse a unary expression: !, unary -, dereference *, address-of &,
 * and postfix array index []
 * postfix [] binds tighter than any prefix operator and is handled here
 * by looping after the base primary is parsed */
static Nd *punary(Lx *l) {
	if (check(l, TK_BANG)) {
		lx_advance(l);
		Nd *n = nd_new(ND_UN);
		n->op = TK_BANG;
		n->a = punary(l);
		return n;
	}
	if (check(l, TK_MINUS)) {
		lx_advance(l);
		Nd *n = nd_new(ND_UN);
		n->op = TK_MINUS;
		n->a = punary(l);
		return n;
	}
	if (check(l, TK_STAR)) {
		lx_advance(l);
		Nd *n = nd_new(ND_DEREF);
		n->a = punary(l);
		return n;
	}
	if (check(l, TK_AMP)) {
		lx_advance(l);
		Nd *n = nd_new(ND_ADDR);
		n->a = punary(l);
		return n;
	}
	Nd *base = pprim(l);
	while (check(l, TK_LBRACKET)) {
		lx_advance(l);
		Nd *idx = pexpr(l);
		expect(l, TK_RBRACKET);
		Nd *n = nd_new(ND_INDEX);
		n->a = base;
		n->b = idx;
		base = n;
	}
	return base;
}

/* parse multiplicative expressions: *, /, %
 * left-associative precedence level 3 (after unary) */
static Nd *pmul(Lx *l) {
	Nd *n = punary(l);
	while (check(l,TK_STAR)||check(l,TK_SLASH)||check(l,TK_PCT)) {
		Tk op = cur(l).t;
		lx_advance(l);
		Nd *r = nd_new(ND_BIN);
		r->op = op;
		r->a = n;
		r->b = punary(l);
		n = r;
	}
	return n;
}

/* parse additive expressions: +, -
 * left-associative precedence level 4 */
static Nd *padd(Lx *l) {
	Nd *n = pmul(l);
	while (check(l,TK_PLUS)||check(l,TK_MINUS)) {
		Tk op = cur(l).t;
		lx_advance(l);
		Nd *r = nd_new(ND_BIN);
		r->op = op;
		r->a = n;
		r->b = pmul(l);
		n = r;
	}
	return n;
}

/* parse relational comparisons: <, <=, >, >=
 * left-associative precedence level 5 */
static Nd *pcmp(Lx *l) {
	Nd *n = padd(l);
	while (check(l,TK_LT)||check(l,TK_LE)||check(l,TK_GT)||check(l,TK_GE)) {
		Tk op = cur(l).t;
		lx_advance(l);
		Nd *r = nd_new(ND_BIN);
		r->op = op;
		r->a = n;
		r->b = padd(l);
		n = r;
	}
	return n;
}

/* parse equality: ==, !=
 * left-associative precedence level 6 */
static Nd *peq(Lx *l) {
	Nd *n = pcmp(l);
	while (check(l,TK_EQ)||check(l,TK_NEQ)) {
		Tk op = cur(l).t;
		lx_advance(l);
		Nd *r = nd_new(ND_BIN);
		r->op = op;
		r->a = n;
		r->b = pcmp(l);
		n = r;
	}
	return n;
}

/* parse logical and: &&
 * left-associative precedence level 7
 * note: && here is bitwise and at the machine level zero is false */
static Nd *pand(Lx *l) {
	Nd *n = peq(l);
	while (check(l,TK_AND)) {
		lx_advance(l);
		Nd *r = nd_new(ND_BIN);
		r->op = TK_AND;
		r->a = n;
		r->b = peq(l);
		n = r;
	}
	return n;
}

/* parse logical or: ||
 * left-associative precedence level 8 (lowest binary) */
static Nd *por(Lx *l) {
	Nd *n = pand(l);
	while (check(l,TK_OR)) {
		lx_advance(l);
		Nd *r = nd_new(ND_BIN);
		r->op = TK_OR;
		r->a = n;
		r->b = pand(l);
		n = r;
	}
	return n;
}

/* parse a full expression, including assignment
 * assignment is right-associative and handled here at the top level
 * lvalue validity is checked: only nd_id, nd_deref, nd_index are legal targets */
static Nd *pexpr(Lx *l) {
	Nd *n = por(l);
	if (check(l, TK_ASSIGN)) {
		lx_advance(l);
		if (n->t == ND_ID) {
			Nd *r = nd_new(ND_ASSIGN);
			r->s = n->s;
			r->a = pexpr(l);
			free(n);
			return r;
		}
		if (n->t == ND_DEREF || n->t == ND_INDEX) {
			Nd *r = nd_new(ND_DEREF_ASSIGN);
			r->a = n;
			r->b = pexpr(l);
			return r;
		}
		die("invalid assignment target");
	}
	return n;
}

/* parse a brace-delimited block of statements
 * returns an nd_block node with statements as children */
static Nd *pblock(Lx *l) {
	expect(l, TK_LBRACE);
	Nd *n = nd_new(ND_BLOCK);
	while (!check(l, TK_RBRACE) && !check(l, TK_EOF)) {
		nd_add_ch(n, pstmt(l));
	}
	expect(l, TK_RBRACE);
	return n;
}

/* parse a single statement: declaration, return, if, while, block, or expression
 * all declarations require an initializer there are no uninitialized variables
 * else-if chains parse naturally: else followed by tk_if recurses into pstmt */
static Nd *pstmt(Lx *l) {
	Tok t = cur(l);
	if (t.t == TK_INT) {
		lx_advance(l);
		int isptr = 0;
		if (check(l, TK_STAR)) { lx_advance(l); isptr = 1; }
		Tok nm = expect(l, TK_ID);
		expect(l, TK_ASSIGN);
		Nd *n = nd_new(ND_DECL);
		n->s = nm.s;
		n->ptr = isptr;
		n->a = pexpr(l);
		expect(l, TK_SEMI);
		return n;
	}
	if (t.t == TK_RETURN) {
		lx_advance(l);
		Nd *n = nd_new(ND_RETURN);
		if (!check(l, TK_SEMI)) n->a = pexpr(l);
		expect(l, TK_SEMI);
		return n;
	}
	if (t.t == TK_IF) {
		lx_advance(l);
		Nd *n = nd_new(ND_IF);
		match(l, TK_LPAREN);
		n->a = pexpr(l);
		match(l, TK_RPAREN);
		n->b = pblock(l);
		if (match(l, TK_ELSE)) {
			if (check(l, TK_IF)) {
				n->c = pstmt(l);
			} else {
				n->c = pblock(l);
			}
		}
		return n;
	}
	if (t.t == TK_WHILE) {
		lx_advance(l);
		Nd *n = nd_new(ND_WHILE);
		match(l, TK_LPAREN);
		n->a = pexpr(l);
		match(l, TK_RPAREN);
		n->b = pblock(l);
		return n;
	}
	if (t.t == TK_LBRACE) {
		return pblock(l);
	}
	Nd *n = pexpr(l);
	expect(l, TK_SEMI);
	return n;
}

/* parse a top-level item: a global variable declaration or a function definition
 * both start with "int" a '(' after the name means function; '=' means global var
 * there is no forward declaration syntax order matters cat is the linker */
static Nd *ptoplevel(Lx *l) {
	if (check(l, TK_INT)) {
		lx_advance(l);
		int isptr = 0;
		if (check(l, TK_STAR)) { lx_advance(l); isptr = 1; }
		if (!check(l, TK_ID)) dief("expected identifier on line %d", l->cur.line);
		Tok nm = expect(l, TK_ID);
		if (check(l, TK_LPAREN)) {
			lx_advance(l);
			Nd *n = nd_new(ND_FN);
			n->s = nm.s;
			n->ptr = isptr;
			n->params = NULL;
			n->npar = 0;
			n->parptrs = NULL;
			if (!check(l, TK_RPAREN)) {
				do {
					expect(l, TK_INT);
					int pp = 0;
					if (check(l, TK_STAR)) { lx_advance(l); pp = 1; }
					Tok p = expect(l, TK_ID);
					n->params = realloc(n->params, (n->npar+1)*sizeof(char*));
					n->parptrs = realloc(n->parptrs, (n->npar+1)*sizeof(int));
					n->params[n->npar] = p.s;
					n->parptrs[n->npar] = pp;
					n->npar++;
				} while (match(l, TK_COMMA));
			}
			expect(l, TK_RPAREN);
			n->a = pblock(l);
			return n;
		}
		expect(l, TK_ASSIGN);
		Nd *n = nd_new(ND_DECL);
		n->s = nm.s;
		n->ptr = isptr;
		n->a = pexpr(l);
		expect(l, TK_SEMI);
		return n;
	}
	return pstmt(l);
}

/* parse the entire source file into a program node (nd_prog) whose children
 * are the top-level declarations and definitions in source order */
Nd *parse(Lx *l) {
	lx_init(l);
	Nd *prog = nd_new(ND_PROG);
	while (!check(l, TK_EOF)) {
		nd_add_ch(prog, ptoplevel(l));
	}
	return prog;
}

/* elf64 header is 64 bytes each phdr is 56 bytes we emit 3 phdrs
 * total header block: 64 + 3*56 = 232 = 0xe8 bytes */
#define ELF_HDR_SZ 64u
#define ELF_PHDR_SZ 56u
#define ELF_NPH 3u /* text, rod, bss */
#define HDRSZ (ELF_HDR_SZ + ELF_PHDR_SZ * ELF_NPH) /* 232 bytes */ 
#define TEXT_SEG 0x400000u /* text segment virtual load address */
#define TEXT_BASE (TEXT_SEG + HDRSZ) /* first instruction lives here */
#define ROD_BASE 0x500000u /* read-only data: string literals */
#define BSS_BASE 0x600000u /* zero-initialized: globals + heap */
#define BSS_SCRATCH 32u /* scratch bytes reserved at the top of bss */
#define HEAP_SZ 65536u /* 64 kb bump-allocator arena */
#define MAX_GLBS 1024 /* maximum number of global variables */
#define BSS_SZ (MAX_GLBS * 4u + BSS_SCRATCH + 4u + HEAP_SZ) /* total bss size in bytes */
#define HEAP_BASE (BSS_BASE + MAX_GLBS * 4u + BSS_SCRATCH) /* first heap byte */
#define BUMP_PTR (HEAP_BASE) /* the bump pointer itself lives at this address */
#define MAX_LOCALS 256 /* max locals per function */
#define MAX_FNS 256 /* max functions per program */
#define MAX_FPATCHES 4096 /* max forward call sites to backpatch */
#define PARAM_BASE 16 /* byte offset from x29 to first spilled param */
#define SLOT_SZ 16 /* every local slot is 16 bytes to keep sp 16-byte aligned */

/* internal codegen state not exposed in the header
 * cg2 holds everything the code generator touches during one compilation */
typedef struct {
	Glb glbs[MAX_GLBS];
	int nglb;
	Buf rod;
	Buf code;
	Fsym fns[MAX_FNS];
	int nfn;
	Fpatch fpatches[MAX_FPATCHES];
	int nfpatch;
} Cg2;
/* local variable environment for one function
 * parallel arrays: name, frame-pointer offset, is-pointer flag
 * lookup scans backwards so inner declarations shadow outer ones */
typedef struct {
	char *names[MAX_LOCALS];
	int offs[MAX_LOCALS];
	int ptrs[MAX_LOCALS];
	int n;
	int frame_sz; /* bytes consumed by locals below x29 (multiple of slot_sz) */
} Lenv;
/* global variable table helpers -*/
/* linear scan for a global by name returns index or -1
 * o(n) lookup with a max of 1024 globals this is perfectly fine */
static int g_find(Cg2 *g, const char *nm) {
	int i;
	for (i=0; i<g->nglb; i++)
		if (!strcmp(g->glbs[i].name, nm)) return i;
	return -1;
}

/* add a new global assigns the next 4-byte bss slot
 * is_const=1 means it was declared with a literal and never written again,
 * so the code generator can inline the value instead of a load */
static int g_add(Cg2 *g, const char *nm, int is_const) {
	if (g->nglb >= MAX_GLBS) die("too many globals");
	g->glbs[g->nglb].name = strdup(nm);
	g->glbs[g->nglb].goff = g->nglb * 4; /* each global is 4 bytes wide */
	g->glbs[g->nglb].is_const = is_const;
	g->glbs[g->nglb].cval = 0;
	g->glbs[g->nglb].is_str = 0;
	g->glbs[g->nglb].soff = 0;
	return g->nglb++;
}

/* append a string literal (with its null terminator) to the read-only data
 * segment returns the byte offset from rod_base where it was placed */
static int rod_add(Cg2 *g, const char *s, int len) {
	int off = (int)g->rod.len;
	buf_append(&g->rod, s, len+1);
	return off;
}

/* current code buffer length in bytes = offset of next instruction from text_base */
static uint32_t cpos(Cg2 *g) {
	return (uint32_t)g->code.len;
}

/* emit one a64 instruction word (32 bits, little-endian) into the code buffer */
static void A(Cg2 *g, uint32_t instr) {
	buf_u32(&g->code, instr);
}

/* aarch64 instruction emitters
 * each function encodes one a64 instruction class directly as a 32-bit word
 * no assembler, no lookup table pure bit manipulation and a() calls
 * the arm architecture reference manual is the source of truth for encodings */

/* load an arbitrary 32-bit immediate into xd
 * movz xd, #lo sets the low 16 bits and zeros the rest
 * movk xd, #hi, lsl #16 fills the high 16 bits when nonzero */
static void a64_mov_imm(Cg2 *g, int rd, uint32_t val) {
	uint32_t lo = val & 0xFFFF;
	uint32_t hi = (val >> 16) & 0xFFFF;
	A(g, (uint32_t)((1u<<31)|(0xA5u<<23)|(0u<<21)|(lo<<5)|(uint32_t)rd));
	if (hi) {
		A(g, (uint32_t)((1u<<31)|(0xE5u<<23)|(1u<<21)|(hi<<5)|(uint32_t)rd));
	}
}

/* mov xd, xm encoded as orr xd, xzr, xm (the canonical alias) */
static void a64_mov_rr(Cg2 *g, int rd, int rm) {
	A(g, (uint32_t)((1u<<31)|(0x2Au<<24)|(0u<<29)|(0u<<22)|(0u<<21)|
	 ((uint32_t)rm<<16)|(0u<<10)|(31u<<5)|(uint32_t)rd));
}

/* add xd, xn, xm 64-bit register add, no shift, no extend */
static void a64_add_rr(Cg2 *g, int rd, int rn, int rm) {
	A(g, (uint32_t)((1u<<31)|(0u<<30)|(0u<<29)|(0x0Bu<<24)|
	 (0u<<22)|(0u<<21)|((uint32_t)rm<<16)|(0u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rd));
}

/* sub xd, xn, xm 64-bit register subtract */
static void a64_sub_rr(Cg2 *g, int rd, int rn, int rm) {
	A(g, (uint32_t)((1u<<31)|(1u<<30)|(0u<<29)|(0x0Bu<<24)|
	 (0u<<22)|(0u<<21)|((uint32_t)rm<<16)|(0u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rd));
}

/* add xd, xn, #imm12 12-bit unsigned immediate, no shift
 * also used as mov sp, xn (add sp, xn, #0) and mov xn, sp (add xn, sp, #0) */
static void a64_add_imm(Cg2 *g, int rd, int rn, uint32_t imm12) {
	A(g, (uint32_t)((1u<<31)|(0u<<30)|(0u<<29)|(0x11u<<24)|
	 (0u<<22)|(imm12<<10)|((uint32_t)rn<<5)|(uint32_t)rd));
}

/* sub xd, xn, #imm12 used for stack allocation and frame teardown */
static void a64_sub_imm(Cg2 *g, int rd, int rn, uint32_t imm12) {
	A(g, (uint32_t)((1u<<31)|(1u<<30)|(0u<<29)|(0x11u<<24)|
	 (0u<<22)|(imm12<<10)|((uint32_t)rn<<5)|(uint32_t)rd));
}

/* mul xd, xn, xm encoded as madd xd, xn, xm, xzr (xd = xn*xm + 0) */
static void a64_mul_rr(Cg2 *g, int rd, int rn, int rm) {
	A(g, (uint32_t)((1u<<31)|(0u<<29)|(0x1Bu<<24)|(0u<<21)|
	 ((uint32_t)rm<<16)|(0u<<15)|(31u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rd));
}

/* udiv xd, xn, xm unsigned integer division xd = xn / xm
 * division by zero produces zero on aarch64 no trap no signal just zero */
static void a64_udiv_rr(Cg2 *g, int rd, int rn, int rm) {
	A(g, (uint32_t)((1u<<31)|(0u<<30)|(0u<<29)|(0xD6u<<21)|
	 ((uint32_t)rm<<16)|(0x02u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rd));
}

/* msub xd, xn, xm, xa multiply-subtract: xd = xa - xn*xm
 * used for remainder: after udiv gives quotient q, msub gives (dividend - q*divisor) */
static void a64_msub_rr(Cg2 *g, int rd, int rn, int rm, int ra) {
	A(g, (uint32_t)((1u<<31)|(0u<<29)|(0x1Bu<<24)|(0u<<21)|
	 ((uint32_t)rm<<16)|(1u<<15)|((uint32_t)ra<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rd));
}

/* cmp xn, xm encoded as subs xzr, xn, xm sets flags, discards result */
static void a64_cmp_rr(Cg2 *g, int rn, int rm) {
	A(g, (uint32_t)((1u<<31)|(1u<<30)|(1u<<29)|(0x0Bu<<24)|
	 (0u<<22)|(0u<<21)|((uint32_t)rm<<16)|(0u<<10)|
	 ((uint32_t)rn<<5)|31u));
}

/* cmp xn, #imm12 encoded as subs xzr, xn, #imm12 */
static void a64_cmp_imm(Cg2 *g, int rn, uint32_t imm12) {
	A(g, (uint32_t)((1u<<31)|(1u<<30)|(1u<<29)|(0x11u<<24)|
	 (0u<<22)|(imm12<<10)|((uint32_t)rn<<5)|31u));
}

/* cset xd, cond encoded as csinc xd, xzr, xzr, ~cond
 * sets xd to 1 if cond is true, 0 otherwise
 * the aarch64 encoding takes the inverted condition, so callers pass:
 * ne=1 for cset eq, eq=0 for cset ne,
 * ge=10 for cset lt, lt=11 for cset ge,
 * gt=12 for cset le, le=13 for cset gt */
static void a64_cset(Cg2 *g, int rd, uint32_t cond_inv) {
	A(g, (uint32_t)((1u<<31)|(0u<<30)|(0u<<29)|(0xD4u<<21)|
	 (31u<<16)|(cond_inv<<12)|(0x1u<<10)|
	 (31u<<5)|(uint32_t)rd));
}

/* str wt, [xn, #byte_offset] unsigned scaled 32-bit store
 * byte_offset must be non-negative and divisible by 4 (the imm12 field is scaled) */
static void a64_str_w(Cg2 *g, int rt, int rn, int byte_offset) {
	uint32_t imm12 = (uint32_t)(byte_offset / 4);
	A(g, (uint32_t)((0x2u<<30)|(0x1Cu<<25)|(1u<<24)|(0x0u<<22)|
	 (imm12<<10)|((uint32_t)rn<<5)|(uint32_t)rt));
}

/* ldr wt, [xn, #byte_offset] unsigned scaled 32-bit load same offset rules */
static void a64_ldr_w(Cg2 *g, int rt, int rn, int byte_offset) {
	uint32_t imm12 = (uint32_t)(byte_offset / 4);
	A(g, (uint32_t)((0x2u<<30)|(0x1Cu<<25)|(1u<<24)|(0x1u<<22)|
	 (imm12<<10)|((uint32_t)rn<<5)|(uint32_t)rt));
}

/* stur wt, [xn, #simm9] unscaled signed 9-bit offset, 32-bit store
 * used for negative offsets (locals below x29) "u" in stur = unscaled, not unsigned */
static void a64_stur_w(Cg2 *g, int rt, int rn, int byte_offset) {
	uint32_t s9 = (uint32_t)(byte_offset) & 0x1FFu;
	A(g, (uint32_t)((0x2u<<30)|(0x1Cu<<25)|(0x0u<<22)|
	 (0u<<21)|(s9<<12)|(0x0u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rt));
}

/* ldur wt, [xn, #simm9] unscaled signed 9-bit offset, 32-bit load */
static void a64_ldur_w(Cg2 *g, int rt, int rn, int byte_offset) {
	uint32_t s9 = (uint32_t)(byte_offset) & 0x1FFu;
	A(g, (uint32_t)((0x2u<<30)|(0x1Cu<<25)|(0x1u<<22)|
	 (0u<<21)|(s9<<12)|(0x0u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rt));
}

/* ldr wt, [xn] zero offset load convenience wrapper */
static void a64_ldr_w0(Cg2 *g, int rt, int rn) {
	a64_ldr_w(g, rt, rn, 0);
}

/* str wt, [xn] zero offset store convenience wrapper */
static void a64_str_w0(Cg2 *g, int rt, int rn) {
	a64_str_w(g, rt, rn, 0);
}

/* ldrb wt, [xn, xm] byte load with register offset, zero-extended to 32 bits
 * used by the bload() intrinsic no scaling: offset is the raw byte index */
static void a64_ldrb_reg(Cg2 *g, int rt, int rn, int rm) {
	A(g, (uint32_t)((0x0u<<30)|(0x1Cu<<25)|(0x1u<<22)|
	 (1u<<21)|((uint32_t)rm<<16)|(0x3u<<13)|
	 (0u<<12)|(0x2u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rt));
}

/* strb wt, [xn, xm] byte store with register offset
 * used by the bstore() intrinsic */
static void a64_strb_reg(Cg2 *g, int rt, int rn, int rm) {
	A(g, (uint32_t)((0x0u<<30)|(0x1Cu<<25)|(0x0u<<22)|
	 (1u<<21)|((uint32_t)rm<<16)|(0x3u<<13)|
	 (0u<<12)|(0x2u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rt));
}

/* strb wt, [xn, #0] byte store at zero offset
 * used in __itoa to write individual digit characters into the scratch buffer */
static void a64_strb_imm0(Cg2 *g, int rt, int rn) {
	A(g, (uint32_t)((0x0u<<30)|(0x1Cu<<25)|(1u<<24)|(0x0u<<22)|
	 (0u<<10)|((uint32_t)rn<<5)|(uint32_t)rt));
}

/* add xd, xn, xm, lsl #2 compute array element address: base + index * 4
 * the language defines sizeof(int) = 4, so all array strides are 4 bytes */
static void a64_add_lsl2(Cg2 *g, int rd, int rn, int rm) {
	A(g, (uint32_t)((1u<<31)|(0u<<30)|(0u<<29)|(0x0Bu<<24)|
	 (0u<<22)|(0u<<21)|((uint32_t)rm<<16)|
	 (2u<<10)|((uint32_t)rn<<5)|(uint32_t)rd));
}

/* neg xd, xm encoded as sub xd, xzr, xm unary minus */
static void a64_neg_rr(Cg2 *g, int rd, int rm) {
	A(g, (uint32_t)((1u<<31)|(1u<<30)|(0u<<29)|(0x0Bu<<24)|
	 (0u<<22)|(0u<<21)|((uint32_t)rm<<16)|(0u<<10)|
	 (31u<<5)|(uint32_t)rd));
}

/* and xd, xn, xm bitwise and used for logical && and bitmask operations */
static void a64_and_rr(Cg2 *g, int rd, int rn, int rm) {
	A(g, (uint32_t)((1u<<31)|(0u<<29)|(0x0Au<<24)|
	 (0u<<22)|(0u<<21)|((uint32_t)rm<<16)|(0u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rd));
}

/* orr xd, xn, xm bitwise or used for logical || */
static void a64_orr_rr(Cg2 *g, int rd, int rn, int rm) {
	A(g, (uint32_t)((1u<<31)|(1u<<29)|(0x0Au<<24)|
	 (0u<<22)|(0u<<21)|((uint32_t)rm<<16)|(0u<<10)|
	 ((uint32_t)rn<<5)|(uint32_t)rd));
}

/* stp x29, x30, [sp, #-16]! push frame pointer and link register
 * pre-indexed: sp decrements by 16 before the store
 * this is the canonical aarch64 function prologue opener */
static void a64_stp_fp_lr(Cg2 *g) {
	A(g, 0xA9BF7BFDu);
}

/* ldp x29, x30, [sp], #16 pop frame pointer and link register
 * post-indexed: loads first, then sp increments by 16
 * paired with a64_stp_fp_lr at every function exit */
static void a64_ldp_fp_lr(Cg2 *g) {
	A(g, 0xA8C17BFDu);
}

/* sub sp, sp, #16 allocate one 16-byte local slot below the stack pointer
 * every local gets 16 bytes even though the value is 32 bits,
 * because sp must stay 16-byte aligned at every bl instruction */
static void a64_push_slot(Cg2 *g) {
	a64_sub_imm(g, 31, 31, 16);
}

/* push w0 onto the stack using a 16-byte slot
 * sp -= 16, then str w0, [sp, #0]
 * used to spill intermediate values across subexpression evaluation */
static void a64_push_w0(Cg2 *g) {
	a64_sub_imm(g, 31, 31, 16); /* sp -= 16 */
	a64_str_w(g, 0, 31, 0); /* [sp] = w0 */
}

/* pop a 32-bit value from the stack into wt
 * ldr wt, [sp, #0], then sp += 16 */
static void a64_pop_wt(Cg2 *g, int rt) {
	a64_ldr_w(g, rt, 31, 0); /* wt = [sp] */
	a64_add_imm(g, 31, 31, 16); /* sp += 16 */
}

/* ret return to the address in x30 encoding: 0xd65f03c0 */
static void a64_ret(Cg2 *g) {
	A(g, 0xD65F03C0u);
}

/* svc #0 supervisor call traps into the kernel to execute the syscall
 * whose number is in x8 arguments in x0-x5 return value in x0 */
static void a64_svc(Cg2 *g) {
	A(g, 0xD4000001u);
}

/* branch patching
 * forward calls and conditional branches are emitted as placeholders (bl #0,
 * b #0, beq #0) and backpatched in a single pass after all code is emitted
 * this is the only reason we need a mutable code buffer */

/* patch the branch instruction at byte offset pos to jump to target (virtual address)
 * detects bcond (0x54xxxxxx) vs b/bl (0x14/0x94xxxxxx) by opcode byte
 * bcond uses a 19-bit pc-relative offset in bits [23:5]
 * b and bl use a 26-bit pc-relative offset in bits [25:0]
 * all a64 branches are in units of 4-byte instructions, so we divide by 4 */
static void patch_b(Buf *b, uint32_t pos, uint32_t target) {
	int32_t off = (int32_t)(target - (TEXT_BASE + pos)) / 4;
	uint32_t existing;
	memcpy(&existing, b->d + pos, 4);
	uint32_t opc = existing & 0xFC000000u; 
	if ((existing & 0xFF000000u) == 0x54000000u) {
		uint32_t cond = existing & 0xFu;
		buf_patch32(b, pos, 0x54000000u | (((uint32_t)off & 0x7FFFFu) << 5) | cond);
	} else {
		buf_patch32(b, pos, opc | ((uint32_t)off & 0x03FFFFFFu));
	}
}

/* emit b #0 (opcode 0x14000000) and return its position for later patching */
static uint32_t emit_b_placeholder(Cg2 *g) {
	uint32_t pos = cpos(g);
	A(g, 0x14000000u); 
	return pos;
}

/* emit beq #0 (opcode 0x54000000, cond=0) for later patching */
static uint32_t emit_beq_placeholder(Cg2 *g) {
	uint32_t pos = cpos(g);
	A(g, 0x54000000u); 
	return pos;
}

/* emit bne #0 (opcode 0x54000001, cond=1) for later patching */
static uint32_t emit_bne_placeholder(Cg2 *g) {
	uint32_t pos = cpos(g);
	A(g, 0x54000001u); 
	return pos;
}

/* record a forward call site: save the name and buffer position, emit bl #0
 * all call patches are resolved in one pass at the end of codegen */
static void a64_bl_placeholder(Cg2 *g, char *nm) {
	if (g->nfpatch >= MAX_FPATCHES) die("too many call patches");
	uint32_t pos = cpos(g);
	g->fpatches[g->nfpatch].pos = pos;
	g->fpatches[g->nfpatch].name = nm;
	g->nfpatch++;
	A(g, 0x94000000u); 
}

/* emit the exit(code) syscall sequence: x8=93, x0=code, svc #0 */
static void a64_syscall_exit(Cg2 *g, int code) {
	a64_mov_imm(g, 8, 93); /* syscall number: exit = 93 */
	a64_mov_imm(g, 0, (uint32_t)code);
	a64_svc(g);
}

/* emit read(0, dst_r, 1): read one byte from stdin into the address in dst_r
 * x8=63 (read), x0=0 (stdin fd), x1=dst_r, x2=1 (one byte) */
static void a64_syscall_read1(Cg2 *g, int dst_r) {
	a64_mov_imm(g, 8, 63); /* syscall number: read = 63 */
	a64_mov_imm(g, 0, 0);
	a64_mov_rr(g, 1, dst_r);
	a64_mov_imm(g, 2, 1);
	a64_svc(g);
}

/* local variable environment*/

/* search for a local variable by name, scanning backwards (innermost first)
 * returns the frame-pointer offset, or -9999 if not found
 * -9999 is not a valid fp offset because frames can't be 9999 bytes deep
 * given our max_locals limit */
static int lenv_find(Lenv *e, const char *nm) {
	int i;
	for (i=e->n-1; i>=0; i--)
		if (!strcmp(e->names[i], nm)) return e->offs[i];
	return -9999;
}

/* register a new local variable with its name, fp offset, and pointer flag */
static void lenv_add(Lenv *e, const char *nm, int off, int ptr) {
	if (e->n >= MAX_LOCALS) die("too many locals");
	e->names[e->n] = strdup(nm);
	e->offs[e->n] = off;
	e->ptrs[e->n] = ptr;
	e->n++;
}

/* value abstraction
 * a val describes where a computed value lives without committing to a register
 * v_imm: a compile-time constant that fits in a movz immediate
 * v_reg: already materialized in x0
 * v_fp: lives on the stack at [x29 + offset] (positive = param, negative = local)
 * v_abs: global variable; x1 holds its absolute bss address, load via [x1]
 * val_rval() materializes any val into x0
 * val_store() stores x0 to any lvalue val
 * keeping values lazy (v_imm, v_fp) avoids redundant loads when the value
 * is immediately used as an lvalue or discarded */

/* construct a compile-time integer constant val */
static Val val_imm(int32_t i) {
	Val v; v.k = V_IMM; v.i = i; return v;
}
/* construct a val indicating the value is already in x0 */
static Val val_reg(void) {
	Val v; v.k = V_REG; v.i = 0; return v;
}
/* construct a val for a stack-resident value at [x29 + off] */
static Val val_fp(int32_t off) {
	Val v; v.k = V_FP; v.i = off; return v;
}
/* construct a val for a global variable whose address is in x1 */
static Val val_abs(void) {
	Val v; v.k = V_ABS; v.i = 0; return v;
}

/* materialize v into x0, emitting the minimum instructions required:
 * v_imm -> movz/movk, v_reg -> nothing, v_fp -> ldr/ldur, v_abs -> ldr [x1] */
static void val_rval(Cg2 *g, Val v) {
	switch (v.k) {
	case V_IMM:
		a64_mov_imm(g, 0, (uint32_t)v.i);
		break;
	case V_REG:
		break;
	case V_FP:
		if (v.i < 0)
			a64_ldur_w(g, 0, 29, v.i);
		else
			a64_ldr_w(g, 0, 29, v.i);
		break;
	case V_ABS:
		a64_ldr_w0(g, 0, 1);
		break;
	}
}

/* store x0 into the lvalue described by lv
 * v_fp positive -> str w0, [x29, #off]; negative -> stur w0, [x29, #off]
 * v_abs -> str w0, [x1] anything else is a bug */
static void val_store(Cg2 *g, Val lv) {
	switch (lv.k) {
	case V_FP:
		if (lv.i < 0)
			a64_stur_w(g, 0, 29, lv.i);
		else
			a64_str_w(g, 0, 29, lv.i);
		break;
	case V_ABS:
		a64_str_w0(g, 0, 1);
		break;
	default:
		die("val_store: not an lvalue");
	}
}

/* resolve a variable name to a val for reading
 * checks locals first (innermost scope wins), then globals
 * constant globals are returned as v_imm and never generate a load
 * string globals return their rod address as v_reg (already in x0)
 * unknown names are a fatal error: simplex has no implicit declarations */
static Val var_rval(Cg2 *g, const char *nm, Lenv *env) {
	int lo = lenv_find(env, nm);
	if (lo != -9999) return val_fp(lo);
	int gi = g_find(g, nm);
	if (gi >= 0) {
		Glb *gl = &g->glbs[gi];
		if (gl->is_const && !gl->is_str) return val_imm((int32_t)gl->cval);
		if (gl->is_const && gl->is_str) {
			a64_mov_imm(g, 0, ROD_BASE + (uint32_t)gl->soff);
			return val_reg();
		}
		a64_mov_imm(g, 1, BSS_BASE + (uint32_t)gl->goff);
		return val_abs();
	}
	dief("undefined variable '%s'", nm);
	return val_reg();
}

/* resolve a variable name to an lvalue val for assignment
 * constants cannot be assigned everything else becomes v_fp or v_abs */
static Val var_lval(Cg2 *g, const char *nm, Lenv *env) {
	int lo = lenv_find(env, nm);
	if (lo != -9999) return val_fp(lo);
	int gi = g_find(g, nm);
	if (gi >= 0) {
		Glb *gl = &g->glbs[gi];
		if (gl->is_const) dief("assignment to const '%s'", nm);
		a64_mov_imm(g, 1, BSS_BASE + (uint32_t)gl->goff);
		return val_abs();
	}
	dief("undefined variable '%s'", nm);
	return val_reg();
}

/* function prologue and epilogue*/

/* emit the aarch64 function prologue:
 * stp x29, x30, [sp, #-16]! save fp and lr
 * mov x29, sp establish frame pointer
 * str wi, [x29, #16 + i*16] spill each parameter from its register
 * parameters are then registered in the local environment as positive fp offsets */
static void emit_fn_entry(Cg2 *g, Lenv *env, char **params, int *parptrs, int npar) {
	int i;
	a64_stp_fp_lr(g); /* STP X29,X30,[SP,#-16]! */
	a64_add_imm(g, 29, 31, 0); /* MOV X29, SP */
	/* allocate param home area BELOW X29 before spilling
	 * the old code used positive offsets [X29+16, X29+32, ...], putting params
	 * ABOVE X29 in the caller's dead zone that is safe as long as no nested
	 * call touches those addresses, but the callee's own emit_fn_entry always
	 * spills its first param to [X29_callee+16] = [SP_at_BL], which is exactly
	 * [SP_caller - 16] = the first slot the expression evaluator pushes into
	 * the result: every recursive call clobbers the caller's saved left operand
	 * of a binary expression, so e.g. n * fact(n-1) reads n-1 instead of n
	 * fix: reserve the param area with an explicit SUB first, then spill with
	 * STUR at negative offsets from X29 SP is now already below all params
	 * before any expression eval push occurs, so no callee can reach them */
	if (npar > 0)
		a64_sub_imm(g, 31, 31, (uint32_t)(npar * SLOT_SZ));
	for (i = 0; i < npar; i++) {
		int off = -(i + 1) * SLOT_SZ; /* negative: below X29 */
		a64_stur_w(g, i, 29, off);
		lenv_add(env, params[i], off, parptrs ? parptrs[i] : 0);
	}
}

/* emit the aarch64 function epilogue:
 * mov sp, x29 restore sp to frame base (unwinds all locals regardless of depth)
 * ldp x29, x30, [sp], #16 restore fp and lr
 * ret branch to x30
 * mov sp, x29 before ldp is the fix for the frame-pointer bug: locals allocated
 * inside branches or loops would leave sp pointing somewhere below x29, and a
 * naive ldp would restore from the wrong address this one instruction makes
 * every function epilogue correct regardless of control flow */
static void emit_fn_exit(Cg2 *g, Lenv *env) {
	(void)env;
	a64_add_imm(g, 31, 29, 0); /* mov sp, x29 */
	a64_ldp_fp_lr(g);
	a64_ret(g);
}

static Val gen_expr(Cg2 *g, Nd *n, Lenv *env);

/* emit a function call with nargs arguments
 * strategy: evaluate each argument left to right into x0, push onto stack
 * then load x0x{n-1} from the stack slots (reverse order since stack grows down)
 * restore sp, then emit bl placeholder
 * every push uses a full 16-byte slot, so sp stays 16-byte aligned at the bl
 * after n pushes: arg[i] is at [sp + (n-1-i)*16]
 * maximum 8 arguments (x0-x7 is the aarch64 argument register limit) */
static void emit_call(Cg2 *g, Nd **args, int nargs, Lenv *env, char *name) {
	int i;
	if (nargs > 8) die("too many arguments (max 8)");
	for (i = 0; i < nargs; i++) {
		Val v = gen_expr(g, args[i], env);
		val_rval(g, v);
		a64_push_w0(g);
	}
	for (i = nargs - 1; i >= 0; i--) {
		a64_ldr_w(g, i, 31, (nargs - 1 - i) * 16);
	}
	if (nargs > 0) {
		a64_add_imm(g, 31, 31, (uint32_t)(nargs * 16));
	}
	a64_bl_placeholder(g, name);
}

/* expression code generator
 * recursively walks an expression ast node and emits aarch64 instructions
 * returns a val describing where the result landed
 * binary operators use the push/pop pattern: left into x0, push, right into x0,
 * pop left into x1, operate result always ends up in x0 */
static Val gen_expr(Cg2 *g, Nd *n, Lenv *env) {
	switch (n->t) {
	case ND_NUM:
		return val_imm((int32_t)n->num);
	case ND_STR:
		a64_mov_imm(g, 0, ROD_BASE + (uint32_t)n->soff);
		return val_reg();
	case ND_ID:
		return var_rval(g, n->s, env);
	case ND_ASSIGN: {
		Val rv = gen_expr(g, n->a, env);
		val_rval(g, rv);
		Val lv = var_lval(g, n->s, env);
		val_store(g, lv);
		return val_reg();
	}
	case ND_UN: {
		Val v = gen_expr(g, n->a, env);
		val_rval(g, v);
		if (n->op == TK_MINUS) {
			a64_neg_rr(g, 0, 0);
		} else if (n->op == TK_BANG) {
			a64_cmp_imm(g, 0, 0);
			a64_cset(g, 0, 1); 
		}
		return val_reg();
	}
	case ND_BIN: {
		Val va = gen_expr(g, n->a, env);
		val_rval(g, va);
		a64_push_w0(g);
		Val vb = gen_expr(g, n->b, env);
		val_rval(g, vb);
		a64_pop_wt(g, 1); 
		switch (n->op) {
		case TK_PLUS:
			a64_add_rr(g, 0, 1, 0);
			break;
		case TK_MINUS:
			a64_sub_rr(g, 0, 1, 0);
			break;
		case TK_STAR:
			a64_mul_rr(g, 0, 1, 0);
			break;
		case TK_SLASH:
			a64_udiv_rr(g, 0, 1, 0);
			break;
		case TK_PCT:
			a64_udiv_rr(g, 2, 1, 0);
			a64_msub_rr(g, 0, 2, 0, 1); 
			break;
		case TK_LT:
			a64_cmp_rr(g, 1, 0);
			a64_cset(g, 0, 10);
			break;
		case TK_LE:
			a64_cmp_rr(g, 1, 0);
			a64_cset(g, 0, 12); 
			break;
		case TK_GT:
			a64_cmp_rr(g, 1, 0);
			a64_cset(g, 0, 13); 
			break;
		case TK_GE:
			a64_cmp_rr(g, 1, 0);
			a64_cset(g, 0, 11); 
			break;
		case TK_EQ:
			a64_cmp_rr(g, 1, 0);
			a64_cset(g, 0, 1); 
			break;
		case TK_NEQ:
			a64_cmp_rr(g, 1, 0);
			a64_cset(g, 0, 0); 
			break;
		case TK_AND:
			a64_and_rr(g, 0, 1, 0);
			break;
		case TK_OR:
			a64_orr_rr(g, 0, 1, 0);
			break;
		default:
			die("unknown binop");
		}
		return val_reg();
	}
	case ND_CALL: {
		if (!strcmp(n->s, "putint")) {
			if (n->nch != 1) die("putint takes 1 argument");
			Val v = gen_expr(g, n->ch[0], env);
			val_rval(g, v);
			a64_bl_placeholder(g, "__out");
			return val_reg();
		}
		if (!strcmp(n->s, "putstr")) {
			if (n->nch != 1) die("putstr takes 1 argument");
			Val v = gen_expr(g, n->ch[0], env);
			val_rval(g, v);
			a64_bl_placeholder(g, "__putstr");
			return val_reg();
		}
		if (!strcmp(n->s, "getc")) {
			if (n->nch != 0) die("getc takes no arguments");
			uint32_t scratch = BSS_BASE + BSS_SZ - 4;
			a64_mov_imm(g, 9, scratch); 
			a64_syscall_read1(g, 9); 
			a64_mov_imm(g, 9, scratch);
			a64_ldr_w0(g, 0, 9); 
			a64_mov_imm(g, 1, 0xFF);
			a64_and_rr(g, 0, 0, 1);
			return val_reg();
		}
		if (!strcmp(n->s, "bload")) {
			if (n->nch != 2) die("bload takes 2 arguments");
			Val vp = gen_expr(g, n->ch[0], env);
			val_rval(g, vp);
			a64_push_w0(g);
			Val vi = gen_expr(g, n->ch[1], env);
			val_rval(g, vi);
			a64_pop_wt(g, 1); 
			a64_ldrb_reg(g, 0, 1, 0); 
			return val_reg();
		}
		if (!strcmp(n->s, "bstore")) {
			if (n->nch != 3) die("bstore takes 3 arguments");
			Val vv = gen_expr(g, n->ch[0], env);
			val_rval(g, vv);
			a64_push_w0(g); 
			Val vp = gen_expr(g, n->ch[1], env);
			val_rval(g, vp);
			a64_push_w0(g); 
			Val vi = gen_expr(g, n->ch[2], env);
			val_rval(g, vi); 
			a64_pop_wt(g, 1); 
			a64_pop_wt(g, 2); 
			a64_strb_reg(g, 2, 1, 0); 
			return val_reg();
		}
		if (!strcmp(n->s, "balloc")) {
			if (n->nch != 1) die("balloc takes 1 argument");
			Val vn = gen_expr(g, n->ch[0], env);
			val_rval(g, vn);
			a64_mov_rr(g, 9, 0); 
			a64_mov_imm(g, 1, BUMP_PTR); 
			a64_ldr_w0(g, 0, 1); 
			a64_push_w0(g); 
			a64_add_imm(g, 9, 9, 3);
			a64_mov_imm(g, 10, ~3u);
			a64_and_rr(g, 9, 9, 10);
			a64_add_rr(g, 0, 0, 9); 
			a64_str_w0(g, 0, 1); 
			a64_pop_wt(g, 0); 
			return val_reg();
		}
		emit_call(g, n->ch, n->nch, env, n->s);
		return val_reg();
	}
	case ND_DEREF: {
		Val v = gen_expr(g, n->a, env);
		val_rval(g, v);
		a64_mov_rr(g, 1, 0);
		a64_ldr_w0(g, 0, 1);
		return val_reg();
	}
	case ND_ADDR: {
		if (n->a->t != ND_ID) die("& requires identifier");
		int lo = lenv_find(env, n->a->s);
		if (lo != -9999) {
			if (lo < 0) {
				a64_sub_imm(g, 0, 29, (uint32_t)(-lo));
			} else {
				a64_add_imm(g, 0, 29, (uint32_t)lo);
			}
			return val_reg();
		}
		int gi = g_find(g, n->a->s);
		if (gi >= 0) {
			Glb *gl = &g->glbs[gi];
			if (gl->is_const && !gl->is_str)
				gl->is_const = 0;
			a64_mov_imm(g, 0, BSS_BASE + (uint32_t)gl->goff);
			return val_reg();
		}
		dief("undefined variable '%s'", n->a->s);
		return val_reg();
	}
	case ND_INDEX: {
		Val vp = gen_expr(g, n->a, env);
		val_rval(g, vp);
		a64_push_w0(g);
		Val vi = gen_expr(g, n->b, env);
		val_rval(g, vi);
		a64_pop_wt(g, 1); 
		a64_add_lsl2(g, 0, 1, 0); 
		a64_ldr_w0(g, 0, 0); 
		return val_reg();
	}
	case ND_DEREF_ASSIGN: {
		Val vval = gen_expr(g, n->b, env);
		val_rval(g, vval);
		a64_push_w0(g); 
		Nd *lhs = n->a;
		if (lhs->t == ND_DEREF) {
			Val vp = gen_expr(g, lhs->a, env);
			val_rval(g, vp);
			a64_mov_rr(g, 1, 0); 
		} else if (lhs->t == ND_INDEX) {
			Val vp = gen_expr(g, lhs->a, env);
			val_rval(g, vp);
			a64_push_w0(g); 
			Val vi = gen_expr(g, lhs->b, env);
			val_rval(g, vi); 
			a64_pop_wt(g, 1); 
			a64_add_lsl2(g, 1, 1, 0); 
		} else {
			die("deref_assign: bad lhs");
		}
		a64_pop_wt(g, 0); 
		a64_str_w0(g, 0, 1); 
		return val_reg();
	}
	default:
		dief("gen_expr: unhandled node %d", n->t);
	}
	return val_reg();
}

/* statement code generator
 * handles blocks, declarations, return, if/else, while, and function definitions
 * in_fn=1 when inside a function body; 0 at top level (global scope)
 * local declarations grow the stack frame downward in 16-byte slots
 * global declarations store their initial value into bss at program startup */
static void gen_stmt(Cg2 *g, Nd *n, Lenv *env, int in_fn) {
	switch (n->t) {
	case ND_BLOCK: {
		int i;
		for (i=0; i<n->nch; i++) gen_stmt(g, n->ch[i], env, in_fn);
		break;
	}
	case ND_DECL: {
		if (!in_fn && n->a->t == ND_STR && !n->ptr) {
			int gi = g_add(g, n->s, 1);
			g->glbs[gi].is_str = 1;
			g->glbs[gi].soff = n->a->soff;
			g->glbs[gi].ptr = 0;
		} else {
			Val v = gen_expr(g, n->a, env);
			val_rval(g, v);
			if (in_fn) {
				env->frame_sz += SLOT_SZ;
				int off = -(env->frame_sz);
				a64_push_slot(g);
				val_store(g, val_fp(off));
				lenv_add(env, n->s, off, n->ptr);
			} else {
				int gi = g_add(g, n->s, 0);
				g->glbs[gi].ptr = n->ptr;
				a64_mov_imm(g, 1, BSS_BASE + (uint32_t)g->glbs[gi].goff);
				a64_str_w0(g, 0, 1);
			}
		}
		break;
	}
	case ND_RETURN:
		if (n->a) {
			Val v = gen_expr(g, n->a, env);
			val_rval(g, v);
		} else {
			a64_mov_imm(g, 0, 0);
		}
		emit_fn_exit(g, env);
		break;
	case ND_IF: {
		Val v = gen_expr(g, n->a, env);
		val_rval(g, v);
		a64_cmp_imm(g, 0, 0);
		uint32_t bfalse = emit_beq_placeholder(g);
		gen_stmt(g, n->b, env, in_fn);
		if (n->c) {
			uint32_t bend = emit_b_placeholder(g);
			patch_b(&g->code, bfalse, TEXT_BASE + cpos(g));
			gen_stmt(g, n->c, env, in_fn);
			patch_b(&g->code, bend, TEXT_BASE + cpos(g));
		} else {
			patch_b(&g->code, bfalse, TEXT_BASE + cpos(g));
		}
		break;
	}
	case ND_WHILE: {
		uint32_t top = TEXT_BASE + cpos(g);
		Val v = gen_expr(g, n->a, env);
		val_rval(g, v);
		a64_cmp_imm(g, 0, 0);
		uint32_t bfalse = emit_beq_placeholder(g);
		gen_stmt(g, n->b, env, in_fn);
		uint32_t bb = emit_b_placeholder(g);
		patch_b(&g->code, bb, top);
		patch_b(&g->code, bfalse, TEXT_BASE + cpos(g));
		break;
	}
	case ND_FN: {
		if (g->nfn >= MAX_FNS) die("too many functions");
		uint32_t skip = emit_b_placeholder(g);
		uint32_t fn_addr = TEXT_BASE + cpos(g);
		g->fns[g->nfn].name = n->s;
		g->fns[g->nfn].addr = fn_addr;
		g->nfn++;
		Lenv fenv;
		memset(&fenv, 0, sizeof(fenv));
		emit_fn_entry(g, &fenv, n->params, n->parptrs, n->npar);
		gen_stmt(g, n->a, &fenv, 1);
		a64_mov_imm(g, 0, 0);
		emit_fn_exit(g, &fenv);
		patch_b(&g->code, skip, TEXT_BASE + cpos(g));
		break;
	}
	default:
		gen_expr(g, n, env);
		break;
	}
}

/* runtime helper functions
 * these three functions (__itoa, __out, __putstr) are emitted before user code
 * they are registered in the function table and called via bl like any other function
 * no libc no printf just syscalls and arithmetic */

/* __itoa(x0=value) -> x0=ptr, x1=length
 * converts a 32-bit unsigned integer to its decimal ascii representation
 * digits are written right-to-left into a 20-byte scratch buffer at the top of bss
 * uses udiv + msub to extract digits without division by 10 being a special case
 * callee-saved registers x19-x22 hold live values across the loop to avoid
 * spilling through the call-clobbered x0-x15 frame: stp x19,x20 + stp x21,x22 */
static void emit_itoa_fn(Cg2 *g) {
	if (g->nfn >= MAX_FNS) die("too many functions");
	g->fns[g->nfn].name = "__itoa";
	g->fns[g->nfn].addr = TEXT_BASE + cpos(g);
	g->nfn++;
	uint32_t scratch = BSS_BASE + BSS_SZ - 20;
	A(g, 0xA9BE53F3u);
	A(g, 0xA9015BF5u);
	a64_mov_rr(g, 19, 0);
	a64_mov_imm(g, 20, scratch + 19);
	a64_mov_rr(g, 21, 20);
	a64_mov_imm(g, 22, 10);
	a64_cmp_imm(g, 19, 0);
	uint32_t bzero = emit_beq_placeholder(g);
	uint32_t loop = TEXT_BASE + cpos(g);
	a64_udiv_rr(g, 23, 19, 22);
	a64_msub_rr(g, 24, 23, 22, 19);
	a64_add_imm(g, 24, 24, (uint32_t)'0');
	a64_strb_imm0(g, 24, 21);
	a64_sub_imm(g, 21, 21, 1);
	a64_mov_rr(g, 19, 23);
	a64_cmp_imm(g, 19, 0);
	uint32_t bne = emit_bne_placeholder(g);
	patch_b(&g->code, bne, loop);
	uint32_t bdone = emit_b_placeholder(g);
	patch_b(&g->code, bzero, TEXT_BASE + cpos(g));
	a64_mov_imm(g, 24, (uint32_t)'0');
	a64_strb_imm0(g, 24, 21);
	a64_sub_imm(g, 21, 21, 1);
	patch_b(&g->code, bdone, TEXT_BASE + cpos(g));
	a64_add_imm(g, 0, 21, 1);
	a64_sub_rr(g, 1, 20, 21);
	A(g, 0xA9415BF5u);
	A(g, 0xA8C253F3u);
	a64_ret(g);
}

/* __out(x0=int value): print an integer as a decimal string to stdout
 * calls __itoa to convert, then issues write(1, ptr, len) via svc #0
 * saves x19 (ptr) and x20 (len) across the __itoa call since bl clobbers x0-x15 */
static void emit_out_fn(Cg2 *g) {
	if (g->nfn >= MAX_FNS) die("too many functions");
	g->fns[g->nfn].name = "__out";
	g->fns[g->nfn].addr = TEXT_BASE + cpos(g);
	g->nfn++;
	A(g, 0xA9BE7BFDu);
	A(g, 0xA90153F3u);
	a64_add_imm(g, 29, 31, 0); 
	a64_bl_placeholder(g, "__itoa");
	a64_mov_rr(g, 19, 0);
	a64_mov_rr(g, 20, 1);
	a64_mov_imm(g, 8, 64); 
	a64_mov_imm(g, 0, 1); 
	a64_mov_rr(g, 1, 19); 
	a64_mov_rr(g, 2, 20); 
	a64_svc(g);
	A(g, 0xA94153F3u);
	A(g, 0xA8C27BFDu);
	a64_ret(g);
}

/* __putstr(x0=char* str): print a null-terminated string to stdout
 * scans forward byte-by-byte to find the length (no strlen, we are strlen),
 * then issues write(1, str, len) via svc #0
 * x19 holds the string pointer, x20 accumulates the byte count */
static void emit_putstr_fn(Cg2 *g) {
	if (g->nfn >= MAX_FNS) die("too many functions");
	g->fns[g->nfn].name = "__putstr";
	g->fns[g->nfn].addr = TEXT_BASE + cpos(g);
	g->nfn++;
	A(g, 0xA9BE7BFDu);
	A(g, 0xA90153F3u);
	a64_add_imm(g, 29, 31, 0); 
	a64_mov_rr(g, 19, 0);
	a64_mov_imm(g, 20, 0);
	uint32_t lp = TEXT_BASE + cpos(g);
	a64_ldrb_reg(g, 0, 19, 20);
	a64_cmp_imm(g, 0, 0);
	uint32_t bz = emit_beq_placeholder(g);
	a64_add_imm(g, 20, 20, 1);
	uint32_t bk = emit_b_placeholder(g);
	patch_b(&g->code, bk, lp);
	patch_b(&g->code, bz, TEXT_BASE + cpos(g));
	a64_mov_imm(g, 8, 64);
	a64_mov_imm(g, 0, 1);
	a64_mov_rr(g, 1, 19);
	a64_mov_rr(g, 2, 20);
	a64_svc(g);
	A(g, 0xA94153F3u); 
	A(g, 0xA8C27BFDu); 
	a64_ret(g);
}

/* top-level codegen entry points*/

/* initialize the public cg struct called once before codegen() */
void cg_init(Cg *cg) {
	buf_init(&cg->code);
	buf_init(&cg->rod);
	cg->goff = 0;
	cg->glbs = NULL;
	cg->nglb = 0;
	cg->entry = TEXT_BASE;
}

/* allocate and zero-initialize the internal cg2 codegen state */
static Cg2 *cg2_new(void) {
	Cg2 *g = calloc(1, sizeof(Cg2));
	buf_init(&g->code);
	buf_init(&g->rod);
	return g;
}

/* walk the ast and intern all string literal nodes into the rod segment
 * each nd_str node gets its soff field set to the byte offset in rod
 * this must run before codegen so that string addresses are known when
 * the code emitter encounters them */
static void resolve_strings(Cg2 *g, Nd *prog) {
	int i;
	if (!prog) return;
	if (prog->t == ND_STR) {
		prog->soff = rod_add(g, prog->s, prog->slen);
		return;
	}
	if (prog->a) resolve_strings(g, prog->a);
	if (prog->b) resolve_strings(g, prog->b);
	if (prog->c) resolve_strings(g, prog->c);
	for (i=0; i<prog->nch; i++) resolve_strings(g, prog->ch[i]);
}

/* main codegen entry point
 * order of emission:
 * 1 __itoa, __out, __putstr (runtime helpers)
 * 2 sp alignment fix-up (4 instructions at program entry)
 * 3 bump allocator initialization
 * 4 top-level statements and function definitions (user code)
 * 5 exit(0) syscall
 * 6 backpatch all forward bl references
 * the entry point (body_entry) is recorded after the runtime helpers
 * so the elf e_entry field points to the first user instruction, not __itoa */
void codegen(Nd *prog, Cg *cg_out) {
	int i, j;
	Cg2 *g = cg2_new();
	resolve_strings(g, prog);
	emit_itoa_fn(g);
	emit_out_fn(g);
	emit_putstr_fn(g);
	uint32_t body_entry = TEXT_BASE + cpos(g);
	A(g, 0x910003E9u); /* add x9, sp, #0 copy sp to x9 without xzr aliasing */
	A(g, 0xB27D013Fu); /* ands xzr, x9, #8 test bit 3 (is sp 16-aligned?) */
	A(g, 0x54000040u); /* beq +8 skip the sub if already aligned */
	A(g, 0xD10023FFu); /* sub sp, sp, #8 drop sp by 8 to reach 16-byte alignment */ 
	a64_mov_imm(g, 0, HEAP_BASE + 4);
	a64_mov_imm(g, 1, BUMP_PTR);
	a64_str_w0(g, 0, 1);
	Lenv env;
	memset(&env, 0, sizeof(env));
	for (i=0; i<prog->nch; i++) gen_stmt(g, prog->ch[i], &env, 0);
	a64_syscall_exit(g, 0);
	for (i=0; i<g->nfpatch; i++) {
		Fpatch *fp = &g->fpatches[i];
		int found = 0;
		for (j=0; j<g->nfn; j++) {
			if (!strcmp(g->fns[j].name, fp->name)) {
				patch_b(&g->code, fp->pos, g->fns[j].addr);
				found = 1;
				break;
			}
		}
		if (!found) dief("undefined function '%s'", fp->name);
	}
	cg_out->code = g->code;
	cg_out->rod = g->rod; /* hand off the string table */
	cg_out->entry = body_entry;
	free(g);
}

/* serialize the compiled code and read-only data into a valid elf64 executable
 * and write it to path no external tools involved we are the linker
 * file layout:
 * [0] 64-byte elf header
 * [64] 56-byte phdr 1: text (pt_load, pf_r|pf_x, vaddr=0x400000)
 * [120] 56-byte phdr 2: rod (pt_load, pf_r, vaddr=0x500000, if any strings)
 * [176] 56-byte phdr 3: bss (pt_load, pf_r|pf_w, vaddr=0x600000, filesz=0)
 * [232] code (text_base)
 * [page-aligned] string literals (rod_base)
 * the text phdr maps from file offset 0, so the elf headers are part of the
 * text segment the kernel loads the whole thing starting at 0x400000 */
void emit_elf(Cg *g, const char *path) {
	uint64_t ehsz = ELF_HDR_SZ;
	uint64_t phsz = ELF_PHDR_SZ;
	uint64_t nph = ELF_NPH;
	uint64_t hdrsz = HDRSZ;
	uint64_t page = 0x1000;
	uint64_t rod_sz = g->rod.len;
	uint64_t entry = g->entry;
	Buf file;
	uint64_t rod_off = 0;
	uint64_t text_filesz;
	uint8_t *E;
	int pi = 1;
	FILE *f;
	buf_init(&file);
	{
		uint64_t i;
		for (i = 0; i < hdrsz; i++) buf_u8(&file, 0);
	}
	buf_append(&file, g->code.d, g->code.len);
	if (rod_sz > 0) {
		while (file.len % page) buf_u8(&file, 0);
		rod_off = (uint64_t)file.len;
		buf_append(&file, g->rod.d, g->rod.len);
	}
	text_filesz = rod_off ? rod_off - hdrsz : (uint64_t)file.len - hdrsz;
	E = file.d;
	E[0]=0x7f; E[1]='E'; E[2]='L'; E[3]='F'; /* elf magic */
	E[4]=2; /* ei_class = elfclass64 */
	E[5]=1; /* ei_data = elfdata2lsb (little-endian) */
	E[6]=1; /* ei_version = ev_current */
	E[7]=0; /* ei_osabi = elfosabi_none */
	le16(E+0x10, 2); /* e_type = et_exec */
	le16(E+0x12, 0xB7); /* e_machine = em_aarch64 */
	le32(E+0x14, 1); /* e_version = ev_current */
	le64(E+0x18, entry); /* e_entry: virtual address of first instruction */
	le64(E+0x20, ehsz); /* e_phoff: phdrs immediately follow the elf header */
	le64(E+0x28, 0); /* e_shoff: no section headers */
	le32(E+0x30, 0); /* e_flags */
	le16(E+0x34, (uint16_t)ehsz); /* e_ehsize = 64 */
	le16(E+0x36, (uint16_t)phsz); /* e_phentsize = 56 */
	le16(E+0x38, (uint16_t)nph); /* e_phnum */
	le16(E+0x3A, 64); /* e_shentsize: irrelevant */
	le16(E+0x3C, 0); /* e_shnum: no sections */
	le16(E+0x3E, 0); /* e_shstrndx: none */
	{
		uint8_t *P = E + ehsz;
		le32(P+0x00, 1); /* pt_load */
		le32(P+0x04, 5); /* pf_r|pf_x */
		le64(P+0x08, 0); /* p_offset: maps from file offset 0 */
		le64(P+0x10, (uint64_t)TEXT_SEG); /* p_vaddr */
		le64(P+0x18, (uint64_t)TEXT_SEG); /* p_paddr */
		le64(P+0x20, hdrsz + text_filesz); /* p_filesz */
		le64(P+0x28, hdrsz + text_filesz); /* p_memsz */
		le64(P+0x30, page); /* p_align */
	}
	if (rod_sz > 0) {
		uint8_t *P = E + ehsz + phsz * (uint64_t)pi++;
		le32(P+0x00, 1);
		le32(P+0x04, 4); /* pf_r (read-only) */
		le64(P+0x08, rod_off);
		le64(P+0x10, (uint64_t)ROD_BASE);
		le64(P+0x18, (uint64_t)ROD_BASE);
		le64(P+0x20, rod_sz);
		le64(P+0x28, rod_sz);
		le64(P+0x30, page);
	}
	{
		uint8_t *P = E + ehsz + phsz * (uint64_t)pi;
		le32(P+0x00, 1);
		le32(P+0x04, 6); /* pf_r|pf_w */
		le64(P+0x08, 0); /* p_offset=0: bss has no file content */
		le64(P+0x10, (uint64_t)BSS_BASE);
		le64(P+0x18, (uint64_t)BSS_BASE);
		le64(P+0x20, 0); /* p_filesz=0: kernel zero-initializes */
		le64(P+0x28, (uint64_t)BSS_SZ); /* p_memsz */
		le64(P+0x30, page);
	}
	f = fopen(path, "wb");
	if (!f) { perror(path); exit(1); }
	if (fwrite(file.d, 1, file.len, f) != file.len) { perror("fwrite"); exit(1); }
	fclose(f);
	chmod(path, 0755);
	fprintf(stderr, "wrote AArch64 ELF64 to %s (%u bytes, entry 0x%x)\n",
	 path, (uint32_t)file.len, (uint32_t)entry);
	free(file.d);
}

/* read an entire file into a null-terminated heap buffer and return it
 * rc = "read c" (or "read code", depending on your mood)
 * fseek/ftell to get the size, malloc once, fread once no streaming
 * we own the whole source before the first token is consumed */
char *rc(const char *path) {
	FILE *f = fopen(path, "rb");
	long n;
	char *buf;
	if (!f) { perror(path); exit(1); }
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	buf = malloc(n+1);
	if (!buf) die("malloc");
	if (fread(buf, 1, n, f) != (size_t)n) { perror("fread"); exit(1); }
	fclose(f);
	buf[n] = 0;
	return buf;
}

/* program entry point parse args, read source, lex, parse, codegen, emit
 * the entire compiler pipeline in eight function calls
 * if anything goes wrong, die() handles it main() does not check for errors
 * because all errors are already fatal by the time they get here */
int main(int argc, char **argv) {
	const char *in;
	const char *out = "a.out";
	char *src;
	Lx *lx;
	Nd *prog;
	Cg cg;
	int i;
	if (argc < 2) {
		fprintf(stderr, "usage: %s src.x -o elf.out\n", argv[0]);
		return 1;
	}
	in = argv[1];
	for (i=2; i<argc; i++) {
		if (!strcmp(argv[i], "-o") && i+1 < argc) out = argv[++i];
		else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
	}
	src = rc(in);
	lx = lx_new(src);
	prog = parse(lx);
	cg_init(&cg);
	codegen(prog, &cg);
	emit_elf(&cg, out);
	free(src);
	return 0;
}

/*
 * you just witnessed pure hackerman magic
 * source text goes in one end a working aarch64 elf64 binary comes out the other
 * the elf header is hand assembled the a64 instructions are hand encoded
 * the bump allocator is three arithmetic operations
 * the linker is cat 
 * simplex2elf: because life is too short for silly build systems
 */
