#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "math.h"
#include "string.h"

//VECTOR

typedef struct {
  unsigned long size;

  unsigned long length;
  char* data;
} vector;

typedef struct {
  vector* vec;

  unsigned long i;
  char rev;
  void* x;
} vector_iterator;

//MAP

#define CONTROL_BYTES 16
#define DEFAULT_BUCKETS 2

typedef struct {
  uint8_t control_bytes[CONTROL_BYTES];
} bucket;

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
  map* map;

  char c;
  unsigned long bucket;

  void* key;
  void* x;
  char current_c;
  bucket* bucket_ref;
} map_iterator;

typedef struct {
  void* val;
  char exists;
} map_insert_result;

//LEXER

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
  token_type tt;
  span s;

  union {
	name* name;
	char* str;
	num* num;
  } val;
} token;

// PARSER

typedef enum {
  move_left, move_right,
  move_for_i, move_for_base,
  move_call_i
} move_kind;

typedef struct exp_idx {
  struct exp_idx* from;
  move_kind kind;
  unsigned long i; //index of substitute
} exp_idx;

/// substituted in reverse
typedef struct {
  vector condition; //vec of sub_conds
  vector val; //expression for every substitute indexes
} substitution;

/// everything has one to three arguments and can be chained
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

/// identifier or expr (empty vec and map)
typedef struct value {
  vector substitutes;
  map substitute_idx;

  struct expr* val;
} value;

typedef struct id {
  span s;
  char* name;
  value val;
  span substitutes;
  unsigned precedence;
} id;

typedef struct reducer {
  char* name;
  unsigned long x;
} reducer;

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
  span pos;
  char x;
} lexer;

typedef struct {
  frontend* fe;
  token current;

  unsigned long pos;

  module* mod;
  map* substitute_idx;
  vector reducers;
} parser;

typedef struct {
  frontend* fe;
  module* mod;

  map scope;
  substitution* sub;
} evaluator;