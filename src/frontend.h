/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
typedef struct {
  char* start;
  char* end;
}span;
typedef struct {
  unsigned long size;

  unsigned long length;
  char* data;
}vector;
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
}map;
typedef struct {
  map ids;
}module;
typedef struct {
  char* file;
  span s;
  unsigned long len;

  vector tokens;

  module global;

  /// tells whether to continue into codegen
  char errored;
}frontend;
void frontend_free(frontend *fe);
#define CONTROL_BYTES 16
typedef struct {
  uint8_t control_bytes[CONTROL_BYTES];
}bucket;
typedef struct {
  map* map;

  char c;
  unsigned long bucket;

  void* key;
  void* x;
  char current_c;
  bucket* bucket_ref;
}map_iterator;
int map_next(map_iterator *iterator);
map_iterator map_iterate(map *map);
void module_free(module *b);
typedef struct id id;
typedef struct {
  char* qualifier;
  char* x;
}name;
typedef struct value value;
typedef struct expr expr;
struct value {
  vector substitutes;
  map substitute_idx;

  struct expr* val;
};
struct id {
  span s;
  char* name;
  value val;
  span substitutes;
  unsigned precedence;
};
void id_free(id *xid);
void map_free(map *map);
void value_free(value *val);
void vector_free(vector *vec);
typedef struct {
  vector* vec;

  unsigned long i;
  char rev;
  void* x;
}vector_iterator;
int vector_next(vector_iterator *iter);
vector_iterator vector_iterate(vector *vec);
void expr_free(expr *e);
int cost(expr *e);
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
}num;
typedef struct {
  vector condition; //condition of substitution
  vector val; //expression for every substitute indexes
}substitution;
typedef struct exp_idx exp_idx;
typedef enum {
  move_left, move_right,
  move_for_i, move_for_base,
  move_call_i
}move_kind;
struct exp_idx {
  struct exp_idx* from;
  move_kind kind;
  unsigned long i; //index of substitute
};
int bind(expr *from,expr *to,substitution *sub,exp_idx *cursor);
struct expr {
  span s;

  struct expr* first;

  int cost; //memoized cost

  enum {
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
	exp_cond, exp_def, exp_for, exp_call //2-3 args
  } kind;

  union {
	struct {
	  enum {
		exp_bind, exp_num, exp_inner
	  } ty;
	  union {
		num *by;
		unsigned long bind;
		struct expr *inner;
	  } val;
	};

	struct {
	  struct expr* base; //if zero

	  char named;
	  unsigned long x;
	  struct expr* i;
	} _for;

	struct {
	  struct value* to;
	  substitution sub;
	} call;
  };
};
void expr_head_free(expr *e);
frontend make_frontend(char *file);
int read_file(frontend *fe,char *filename);
typedef enum {
  t_name, t_non_bind,
  t_add, t_sub,
  t_ellipsis, t_comma,
  t_in, t_for,
  t_eq, t_lparen, t_rparen,
  t_str, t_num,
  t_sync, t_eof
}token_type;
typedef struct {
  token_type tt;
  span s;

  union {
	name* name;
	char* str;
	num* num;
  } val;
}token;
int is_name(token *x);
void token_free(token *t);
void lex(frontend *fe);
typedef struct {
  frontend* fe;
  span pos;
  char x;
}lexer;
int lex_char(lexer *l);
void lex_name(lexer *l);
extern name EQ_NAME;
extern name SUB_NAME;
extern name ADD_NAME;
extern const char *RESERVED;
num *num_new(num x);
token *token_push(lexer *l,token_type tt);
int lex_next_eq(lexer *l,char x);
char lex_peek(lexer *l);
void lex_mark(lexer *l);
void lex_back(lexer *l);
int lex_next(lexer *l);
int lex_eof(lexer *l);
void print_num(num *n);
void *heapcpy(size_t size,const void *val);
void note(const span *s,const char *x);
void warn(const span *s,const char *x);
int throw(const span *s,const char *x);
void *vector_get(vector *vec,unsigned long i);
void *vector_push(vector *vec);
vector vector_new(unsigned long size);
#if _WIN32
void set_col(FILE *f,char color);
#endif
#if !(_WIN32)
void set_col(FILE *f,char color);
#endif
void msg(frontend *fe,const span *s,char color1,char color2,const char *template_empty,const char *template,const char *msg);
char *spanstr(span *s);
void *heap(size_t size);
void *heap(size_t size);
unsigned long spanlen(span *s);
int spaneq(span s,char *x);
int compare_name(name *x1,name *x2);
uint64_t hash_string(char **x);
uint64_t hash_name(name *x);
extern const span SPAN_NULL;
extern frontend *FRONTEND;
