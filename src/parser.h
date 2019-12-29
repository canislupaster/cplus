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
}map;
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
typedef struct {
  map ids;
}module;
void print_module(module *b);
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
void print_num(num *n);
typedef struct expr expr;
void print_expr(expr *e);
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
  char* file;
  span s;
  unsigned long len;

  vector tokens;

  module global;

  /// tells whether to continue into codegen
  char errored;
}frontend;
void parse(frontend *fe);
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
  char* qualifier;
  char* x;
}name;
typedef struct {
  token_type tt;
  span s;

  union {
	name* name;
	char* str;
	num* num;
  } val;
}token;
typedef struct {
  frontend* fe;
  token current;

  unsigned long pos;

  module* mod;
  map* substitute_idx;
  vector reducers;
}parser;
int parse_mod(parser *p,module *b);
void module_init(module *b);
void map_configure_string_key(map *map,unsigned long size);
int parse_id(parser *p);
char *isprintf(const char *fmt,...);
vector vector_new(unsigned long size);
void note(const span *s,const char *x);
int map_remove(map *map,void *key);
int vector_pop(vector *vec);
typedef struct {
  void* val;
  char exists;
}map_insert_result;
map_insert_result map_insertcpy(map *map,void *key,void *v);
expr *parse_expr(parser *p,int do_bind,unsigned op_prec);
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
expr *parse_left_expr(parser *p,int bind);
typedef struct reducer reducer;
struct reducer {
  char* name;
  unsigned long x;
};
unsigned long *unqualified_access(parser *p,char *x);
typedef struct id id;
typedef struct value value;
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
id *id_access(parser *p,name *x);
int try_parse_unqualified(parser *p);
int is_name(token *x);
int try_parse_name(parser *p);
void map_configure_ulong_key(map *map,unsigned long size);
map map_new();
map_insert_result map_insert(map *map,void *key);
int num_gt(num num1,num than);
void *map_find(map *map,void *key);
void reduce(expr **exp);
num num_pow(num num1,num num2);
num num_div(num num1,num num2);
num mul(num num1,num num2);
num add(num num1,num num2);
num invert(num n);
num *num_new(num x);
void set_num(expr *e,num n);
extern const int CALL_COST;
expr *goto_idx(expr *root,exp_idx *where);
void expr_head_free(expr *e);
void expr_free(expr *e);
int remove_num(expr **eref,num *num);
int replace_binding(expr *e,unsigned long x1,expr *e2);
int binding_exists(expr *e,unsigned long x);
int substitute(expr *exp,substitution *sub);
void exp_rename(expr *exp,unsigned offset);
typedef struct {
  vector* vec;

  unsigned long i;
  char rev;
  void* x;
}vector_iterator;
int vector_next(vector_iterator *iter);
vector_iterator vector_iterate(vector *vec);
void vector_cpy(vector *from,vector *to);
void *heapcpy(size_t size,const void *val);
expr *exp_copy(expr *exp);
int num_eq(num num1,num num2);
typedef struct {
  exp_idx where;
}sub_cond;
void *vector_pushcpy(vector *vec,void *x);
exp_idx *descend_i(exp_idx *start,move_kind kind,unsigned long i);
exp_idx *descend(exp_idx *start,move_kind kind);
int binary(expr *exp);
extern num ZERO;
int cost(expr *e);
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
extern const expr EXPR_ZERO;
extern num ONE;
extern const expr EXPR_ONE;
expr *expr_new_p(parser *p,expr *first);
void *heap(size_t size);
void *heap(size_t size);
expr *expr_new(expr *first);
void synchronize(parser *p);
int separator(parser *p);
int parse_next_eq(parser *p,token_type tt);
int parse_sync(parser *p);
int peek_sync(parser *p);
void parse_next(parser *p);
token *parse_peek(parser *p);
void *vector_get(vector *vec,unsigned long i);
token *parse_peek_x(parser *p,int x);
int throw(const span *s,const char *x);
int throw_here(parser *p,const char *x);
