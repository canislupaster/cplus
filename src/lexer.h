/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void drop(void* ptr);
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

typedef enum {
	t_name, t_non_bind,
	t_add, t_sub,
	t_ellipsis, t_comma,
	t_in, t_for,
	t_eq, t_lparen, t_rparen,
	t_str, t_num,
	t_sync, t_eof
} token_type;
typedef struct span span;
typedef struct module module;
typedef struct {
	char* qualifier;
	char* x;
} name;
typedef struct {
	unsigned long size;

	unsigned long length;
	char* data;
} vector;
typedef struct {
	unsigned long key_size;
	unsigned long size;

	/// hash and compare
	uint64_t (* hash)(void*);

	/// compare(&left, &right)
	int (* compare)(void*, void*);

	unsigned long length;
	unsigned long num_buckets;
	char* buckets;
} map;
struct module {
	char* name;

	span s;
	vector tokens;

	map ids;
};
struct span {
	module* mod;

	char* start;
	char* end;
};
typedef struct {
	enum {
		num_decimal,
		num_integer,
	} ty;

	union {
		uint64_t uint;
		int64_t integer;
		long double decimal;
	};
} num;
typedef struct {
	token_type tt;
	span s;

	union {
		name* name;
		char* str;
		num* num;
	} val;
} token;

void token_free(token* t);

typedef struct {
	module current;

	char errored; //whether to continue into next stage (ex. interpreter/codegen)

	map allocations; //ptr to trace
} frontend;

void lex(frontend* fe);

typedef struct {
	module* mod;
	span pos;
	char x;
} lexer;

int lex_char(lexer* l);

int throw(const span* s, const char* x);

unsigned long span_len(span* s);

int span_eq(span s, char* x);

char* spanstr(span* s);

void lex_name(lexer* l, char state);

extern name EQ_NAME;
extern name SUB_NAME;
extern name ADD_NAME;
extern const char* SKIP;

void* heapcpy(size_t size, const void* val);

num* num_new(num x);

void* vector_pushcpy(vector* vec, void* x);

token* token_push(lexer* l, token_type tt);

int lex_next_eq(lexer* l, char x);

char lex_peek(lexer* l);

void lex_mark(lexer* l);

void lex_back(lexer* l);

int lex_eof(lexer* l);

int lex_next(lexer* l);
