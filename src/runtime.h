/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
typedef struct expr expr;

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

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

int binary(expr* exp);

typedef struct id id;
typedef struct {
	char* qualifier;
	char* x;
} name;
typedef struct {
	unsigned long size;

	unsigned long length;
	char* data;
} vector;
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

void print_expr(expr* exp);
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

vector vector_new(unsigned long size);

map map_new();

typedef struct value value;
struct value {
	span s;
	vector groups; //conditions for substitutes in each expression
	vector substitutes; //vector of sub_idx specifying substitutes

	map substitute_idx;

	struct expr* exp;
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

extern char* MAIN;

num num_pow(num num1, num num2);

num num_div(num num1, num num2);

num num_mul(num num1, num num2);

num num_add(num num1, num num2);

void set_num(expr* e, num n);

num num_invert(num n);

num* num_new(num x);

int unary(expr* exp);

int is_value(expr* exp);

void exp_rename(expr* exp, unsigned threshold, unsigned offset);

expr exp_copy_value(expr* exp);

typedef struct {
	struct value* to;
	char static_; //whether it can be inlined / passes all conditions statically
	vector val; //expression for every substitute indexes
} substitution;

expr* get_sub(substitution* sub, unsigned long i);

void* vector_pushcpy(vector* vec, void* x);

typedef struct {
	vector* vec;

	unsigned long i;
	char rev;
	void* x;
} vector_iterator;

int vector_next(vector_iterator* iter);

vector_iterator vector_iterate(vector* vec);

extern num ZERO;

int num_eq(num num1, num num2);

int map_remove(map* map, void* key);

typedef struct {
	void* val;
	char exists;
} map_insert_result;

map_insert_result map_insertcpy(map* map, void* key, void* v);

int throw(const span* s, const char* x);

int def(expr* exp);

int condition(substitution* sub, unsigned long i, expr* root);

typedef struct {
	frontend* fe;
	module* mod;

	unsigned long stack_offset; //length of sub

	map scope;
	//vector of copied substitutes for lazy evaluation
	int bind;
	vector sub;
} evaluator;

int ev_condition(evaluator* ev, substitution* sub, unsigned long i);

void* map_find(map* map, void* key);

int evaluate(evaluator* ev, expr* exp, expr* out);

expr* expr_new();

int is_literal(expr* exp);

void* vector_get(vector* vec, unsigned long i);

expr* ev_unqualified_access(evaluator* ev, unsigned int x);
