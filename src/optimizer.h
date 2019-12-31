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
typedef struct expr expr;
expr* extract_operand(expr* exp, unsigned long x1);
void* map_find(map* map, void* key);
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
int num_eq(num num1, num num2);
void reduce(expr** exp);
extern num ZERO;
extern num ONE;
int remove_num(expr** eref, num* num);
num num_pow(num num1, num num2);
num num_div(num num1, num num2);
num num_mul(num num1, num num2);
num num_add(num num1, num num2);
void set_num(expr* e, num n);
void expr_free(expr* exp);
num num_invert(num n);
num* num_new(num x);
expr* expr_new(expr* first);
typedef struct {
  unsigned long size;

  unsigned long length;
  char* data;
} vector;
typedef struct {
  char* start;
  char* end;
} span;
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
typedef struct {
  void* val;
  char exists;
} map_insert_result;
map_insert_result map_insert(map* map, void* key);
int binary(expr* exp);
int is_value(expr* exp);
typedef struct {
  vector* vec;

  unsigned long i;
  char rev;
  void* x;
} vector_iterator;
int vector_next(vector_iterator* iter);
vector_iterator vector_iterate(vector* vec);
int cost(expr* e);
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
extern const int CALL_COST;
