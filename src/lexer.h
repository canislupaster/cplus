/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
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
typedef struct {
  char* start;
  char* end;
} span;
typedef struct {
  char* qualifier;
  char* x;
} name;
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
typedef struct {
  map ids;
} module;
typedef struct {
  char* file;
  span s;
  unsigned long len;

  vector tokens;

  module global;

  /// tells whether to continue into codegen
  char errored;
} frontend;
void lex(frontend* fe);
typedef struct {
  frontend* fe;
  span pos;
  char x;
} lexer;
int lex_char(lexer* l);
int throw(const span* s, const char* x);
unsigned long spanlen(span* s);
int spaneq(span s, char* x);
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
