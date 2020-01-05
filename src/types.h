#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "math.h"
#include "string.h"

#define TRACE_SIZE 10

typedef struct {
	void* stack[TRACE_SIZE];
} trace;

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

typedef struct module module;
typedef struct span {
	module* mod;

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
	move_left, move_right, move_inner,
	move_for_i, move_for_base, move_for_step,
	move_call_i
} move_kind;

/// substituted in reverse
typedef struct {
	struct value* to;
	char static_; //whether it can be inlined / passes all conditions statically
	vector val; //expression for every substitute indexes
} substitution;

typedef enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
			exp_cond, exp_def, exp_for, exp_call //2-3 args
} kind;

/// everything has one to three arguments and can be chained
typedef struct expr {
	span s;
	int cost; //memoized cost

	kind kind;

	union {
		num* by;
		unsigned long bind;
		struct expr* inner;

		struct {
			struct expr* base; //if zero
			struct expr* step;

			char named;
			unsigned long x;
			struct expr* i;
		} _for;

		struct {
			struct expr* left;
			struct expr* right;
		} binary;

		struct {
			struct id* to;
			vector sub; // multiple dispatch, iterate until condition checks
		} call;
	};
} expr;

typedef struct exp_idx {
	struct exp_idx* from;
	move_kind kind;
	unsigned long i; //index of value
	unsigned long i2; //index of substitute
} exp_idx;

typedef struct expr_iterator {
	expr* root;
	expr* x;
	exp_idx* cursor;

	vector sub_done; //left, then right, (or step, base, i) then up the cursor, pop done
	char done;
} expr_iterator;

typedef struct {
	expr* exp;
	expr* x;
	expr* other;

	char right;
} binary_iterator;

typedef struct sub_idx {
	unsigned int i;
	exp_idx* idx;
} sub_idx;

typedef struct {
	exp_idx* idx;

	expr* exp; //make sure it is equivalent to an expression at leaf-binding
	kind kind; //otherwise check kind and descend through substitute indexes
} sub_cond;

typedef struct sub_group {
	vector condition;
} sub_group;

/// identifier or expr (empty vec and map)
typedef struct value {
	span s;
	vector groups; //conditions for substitutes in each expression
	vector substitutes; //vector of sub_idx specifying substitutes

	map substitute_idx;

	struct expr* exp;
} value;

typedef struct id {
	span s;
	char* name;
	vector val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
} id;

typedef struct reducer {
	char* name;
	unsigned long x;
} reducer;

struct module {
	char* name;

	span s;
	vector tokens;

	map ids;
};

typedef struct {
	module current;

	char errored; //whether to continue into next stage (ex. interpreter/codegen)

	map allocations; //ptr to trace
} frontend;

typedef struct {
	module* mod;
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

	unsigned long stack_offset; //length of sub

	map scope;
	//vector of copied substitutes for lazy evaluation
	int bind;
	vector sub;
} evaluator;