/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
typedef struct {
  unsigned long size;

  unsigned long length;
  char* data;
} vector;
void vector_free(vector* vec);
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
void print_num(num* n);
typedef struct expr expr;
void print_expr(expr* exp);
int remove_num(expr** eref, num* num);
void expr_free(expr* exp);
void expr_head_free(expr* exp);
expr* extract_operand(expr* exp, unsigned long x1);
int binding_exists(expr* exp, unsigned long x);
typedef struct {
  char* start;
  char* end;
} span;
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
typedef struct {
  frontend* fe;
  module* mod;

  map scope;
  substitution* sub;
} evaluator;
int condition(evaluator* ev, expr* from, expr* to);
typedef struct {
  vector condition; //condition of substitution
  vector val; //expression for every substitute indexes
} substitution;
int substitute(expr* exp, substitution* sub);
int num_eq(num num1, num num2);
int cost(expr* e);
typedef struct exp_idx exp_idx;
typedef enum {
  move_left, move_right,
  move_for_i, move_for_base,
  move_call_i
} move_kind;
struct exp_idx {
  struct exp_idx* from;
  move_kind kind;
  unsigned long i; //index of substitute
};
int bind(expr* from, expr* to, substitution* sub, exp_idx* cursor);
typedef struct value value;
struct value {
  vector substitutes;
  map substitute_idx;

  struct expr* val;
};
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
		num* by;
		unsigned long bind;
		struct expr* inner;
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
typedef struct {
  unsigned int x;
  struct expr what;
} sub_cond;
void* vector_pushcpy(vector* vec, void* x);
void* vector_get(vector* vec, unsigned long i);
expr* goto_idx(expr* root, exp_idx* where);
exp_idx* descend_i(exp_idx* start, move_kind kind, unsigned long i);
exp_idx* descend(exp_idx* start, move_kind kind);
void exp_rename(expr* exp, unsigned offset);
typedef struct {
  vector* vec;

  unsigned long i;
  char rev;
  void* x;
} vector_iterator;
int vector_next(vector_iterator* iter);
vector_iterator vector_iterate(vector* vec);
void vector_cpy(vector* from, vector* to);
void* heapcpy(size_t size, const void* val);
expr* exp_copy(expr* exp);
int binary(expr* exp);
int is_value(expr* exp);
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
  char* qualifier;
  char* x;
} name;
typedef struct {
  token_type tt;
  span s;

  union {
	name* name;
	char* str;
	num* num;
  } val;
} token;
typedef struct {
  frontend* fe;
  token current;

  unsigned long pos;

  module* mod;
  map* substitute_idx;
  vector reducers;
} parser;
expr* expr_new_p(parser* p, expr* first);
void* heap(size_t size);
expr* expr_new(expr* first);
