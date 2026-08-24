/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#ifndef SIMPLEX2ELF_H
#define SIMPLEX2ELF_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
	TK_NUM,
	TK_STR,
	TK_ID,
	TK_INT,
	TK_IF,
	TK_ELSE,
	TK_WHILE,
	TK_RETURN,
	TK_PLUS,
	TK_MINUS,
	TK_STAR,
	TK_SLASH,
	TK_PCT,
	TK_EQ,
	TK_NEQ,
	TK_LT,
	TK_LE,
	TK_GT,
	TK_GE,
	TK_AND,
	TK_OR,
	TK_BANG,
	TK_ASSIGN,
	TK_AMP,
	TK_LPAREN,
	TK_RPAREN,
	TK_LBRACE,
	TK_RBRACE,
	TK_LBRACKET,
	TK_RBRACKET,
	TK_SEMI,
	TK_COMMA,
	TK_EOF
} Tk;

typedef struct {
	Tk t;
	long num;
	char *s;
	int slen;
	int line;
} Tok;

typedef struct {
	uint8_t *d;
	size_t cap, len;
} Buf;

typedef struct Nd Nd;

typedef enum {
	ND_NUM,
	ND_STR,
	ND_ID,
	ND_DECL,
	ND_ASSIGN,
	ND_BIN,
	ND_UN,
	ND_DEREF,
	ND_ADDR,
	ND_INDEX,
	ND_DEREF_ASSIGN,
	ND_BLOCK,
	ND_IF,
	ND_WHILE,
	ND_FN,
	ND_CALL,
	ND_RETURN,
	ND_PROG
} Nt;

typedef struct Nd {
	Nt t;
	long num;
	char *s;
	int slen;
	int soff;
	int ptr;
	Tk op;
	struct Nd *a, *b, *c;
	struct Nd **ch;
	int nch;
	char **params;
	int *parptrs;
	int npar;
} Nd;

typedef struct {
	char *name;
	int off;
	int ptr;
} Loc;

typedef struct {
	char *name;
	int goff;
	int is_const;
	long cval;
	int is_str;
	int soff;
	int ptr;
} Glb;

typedef struct {
	Loc *locs;
	int nloc;
	int sz;
} Fr;

typedef struct {
	char *src;
	int pos;
	int line;
	Tok cur;
	Tok peek;
} Lx;

typedef struct {
	Buf code;
	Buf rod;
	int goff;
	Glb *glbs;
	int nglb;
	uint32_t entry;
} Cg;

typedef struct {
	char *name;
	uint32_t addr;
} Fsym;

typedef struct {
	Fsym *fns;
	int nfn;
} Ft;

typedef struct {
	uint32_t pos;
	char *name;
} Fpatch;

typedef struct {
	Fpatch *p;
	int np;
} Fpl;

typedef enum { V_IMM, V_REG, V_FP, V_ABS } Vk;
typedef struct { Vk k; int32_t i; } Val;

void buf_init(Buf *b);
void buf_reserve(Buf *b, size_t n);
void buf_append(Buf *b, const void *s, size_t n);
void buf_u8(Buf *b, uint8_t x);
void buf_u32(Buf *b, uint32_t x);
void buf_patch32(Buf *b, size_t off, uint32_t x);
Lx *lx_new(char *src);
void lx_advance(Lx *l);
Tok lx_next(Lx *l);
Nd *parse(Lx *l);
Nd *nd_new(Nt t);
void cg_init(Cg *g);
void codegen(Nd *prog, Cg *g);
void emit_elf(Cg *g, const char *path);
char *rc(const char *path);

#endif
