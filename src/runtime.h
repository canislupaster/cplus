/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
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
void map_configure_ulong_key(map* map, unsigned long size);
map map_new();
typedef struct id id;
typedef struct {
  char* start;
  char* end;
} span;
typedef struct {
  char* qualifier;
  char* x;
} name;
typedef struct value value;
typedef struct {
  unsigned long size;

  unsigned long length;
  char* data;
} vector;
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
void evaluate_main(frontend* fe);
int cost(expr* e);
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
num num_invert(num n);
num num_pow(num num1, num num2);
num num_div(num num1, num num2);
num num_mul(num num1, num num2);
num num_add(num num1, num num2);
num* num_new(num x);
int binary(expr* exp);
int map_remove(map* map, void* key);
typedef struct {
  void* val;
  char exists;
} map_insert_result;
map_insert_result map_insertcpy(map* map, void* key, void* v);
int throw(const span* s, const char* x);
int num_eq(num num1, num num2);
typedef struct {
  vector* vec;

  unsigned long i;
  char rev;
  void* x;
} vector_iterator;
int vector_next(vector_iterator* iter);
vector_iterator vector_iterate(vector* vec);
int evaluate(evaluator* ev, expr* exp, expr* out);
expr rhs(evaluator* ev, expr* exp);
void* map_find(map* map, void* key);
void* vector_get(vector* vec, unsigned long i);
expr ev_unqualified_access(evaluator* ev, unsigned int x);
