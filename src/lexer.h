/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

void drop(void* ptr);

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

typedef struct span span;
typedef struct module module;
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

void token_free(token* t);

void lex(frontend* fe);

int lex_char(lexer* l);

int throw(const span* s, const char* x);

unsigned long span_len(span* s);

int span_eq(span s, char* x);

char* spanstr(span* s);

void lex_name(lexer* l, char state);

extern name EQ_NAME;
extern name SUB_NAME;
extern name ADD_NAME;
extern const char* RESERVED;

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
