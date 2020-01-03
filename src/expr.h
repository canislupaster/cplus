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

typedef struct expr expr;
typedef struct {
	char* start;
	char* end;
} span;

int cost(expr* exp);

enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
			exp_cond, exp_def, exp_for, exp_call //2-3 args
};
typedef enum kind kind;
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
typedef struct value value;
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
	//vector of copied substitutes for lazy evaluation
	int bind;
	vector sub;
} evaluator;

int condition(evaluator* ev, expr* exp1, expr* exp2);

struct value {
	span s;
	vector condition;
	vector substitutes; //vector of sub_idx specifying substitutes in terms of move_call_i

	map substitute_idx;

	struct expr* exp;
};
typedef struct {
	struct value* to;
	vector condition; //vec of sub_conds
	vector val; //expression for every substitute indexes
} substitution;

int bind(expr* from, expr* to, substitution* sub);

int binary(expr* exp);

typedef struct id id;
typedef struct {
	char* qualifier;
	char* x;
} name;
struct id {
	span s;
	char* name;
	vector val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
};
struct expr {
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
};

void expr_free(expr* exp);

void print_num(num* n);

void print_expr(expr* exp);

int num_eq(num num1, num num2);

int remove_num(expr** eref, num* num);

expr* extract_operand(expr* exp, unsigned long x);

typedef struct {
	expr* exp;
	expr* x;
	expr* other;

	char right;
} binary_iterator;

int binary_next(binary_iterator* iter);

binary_iterator binary_iter(expr* exp);

int binding_exists(expr* exp, unsigned long x);

int substitute(expr* exp, substitution* sub);

typedef struct exp_idx exp_idx;
typedef enum {
	move_left, move_right, move_inner,
	move_for_i, move_for_base, move_for_step,
	move_call_i
} move_kind;
struct exp_idx {
	struct exp_idx* from;
	move_kind kind;
	unsigned long i; //index of value
	unsigned long i2; //index of substitute
};
typedef struct {
	unsigned int i; //substitute index
	exp_idx idx;

	expr* exp; //make sure it is equivalent to an expression at leaf-binding
	kind kind; //otherwise check kind and descend through substitute indexes
} sub_cond;
typedef struct sub_idx sub_idx;
struct sub_idx {
	unsigned int i;
	exp_idx idx;
};

void vector_setcpy(vector* vec, unsigned long i, void* x);

void gen_substitutes(value* val, unsigned int i);

int is_literal(expr* exp);

void gen_condition(value* val, unsigned int i);

void exp_rename(expr* exp, unsigned threshold, unsigned offset);

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

typedef struct expr_iterator expr_iterator;
struct expr_iterator {
	expr* root;
	expr* x;
	exp_idx* cursor;

	vector sub_done; //left, then right, (or step, base, i) then up the cursor, pop done
	char done;
};

int exp_next(expr_iterator* iter);

void* vector_pushcpy(vector* vec, void* x);

void exp_go(expr_iterator* iter);

int exp_get(expr_iterator* iter);

int vector_pop(vector* vec);

void exp_ascend(expr_iterator* iter);

expr_iterator exp_iter(expr* exp);

void* vector_get(vector* vec, unsigned long i);

expr* goto_idx(expr* root, exp_idx* where);

exp_idx* descend_i(exp_idx* start, move_kind kind, unsigned long i, unsigned long i2);

exp_idx* descend(exp_idx* start, move_kind kind);

int def(expr* exp);

int unary(expr* exp);

int is_value(expr* exp);

vector vector_new(unsigned long size);

void call_new(expr* exp, id* i);

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

expr* expr_new();
