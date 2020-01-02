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
typedef struct {
	frontend* fe;
	module* mod;

	map scope;
	//vector of copied substitutes for lazy evaluation
	vector sub;
} evaluator;

int condition(evaluator* ev, expr* exp1, expr* exp2);

typedef struct {
	vector condition; //vec of sub_conds
	vector val; //expression for every substitute indexes
} substitution;

int bind(expr* from, expr* to, substitution* sub);

int binary(expr* exp);

typedef struct value value;
struct value {
	vector substitutes;
	map substitute_idx;

	struct expr* val;
};
struct expr {
	span s;
	int cost; //memoized cost

	enum {
		exp_bind, exp_num,
		exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
		//a conditional is a for expressed without the base, def is a for if i=1
				exp_cond, exp_def, exp_for, exp_call //2-3 args
	} kind;

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
			struct value* to;
			substitution sub;
		} call;
	};
};

void print_expr(expr* exp);

void map_configure_ulong_key(map* map, unsigned long size);

vector vector_new(unsigned long size);

map map_new();

typedef struct id id;
typedef struct {
	char* qualifier;
	char* x;
} name;
struct id {
	span s;
	char* name;
	value val;
	span substitutes;
	unsigned precedence;
};

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

void vector_clear(vector* vec);

typedef struct {
	unsigned int x;
	struct expr* what;
} sub_cond;
typedef struct {
	vector* vec;

	unsigned long i;
	char rev;
	void* x;
} vector_iterator;

int vector_next(vector_iterator* iter);

vector_iterator vector_iterate(vector* vec);

void vector_cpy(vector* from, vector* to);

extern num ZERO;

int map_remove(map* map, void* key);

typedef struct {
	void* val;
	char exists;
} map_insert_result;

map_insert_result map_insertcpy(map* map, void* key, void* v);

int throw(const span* s, const char* x);

int num_eq(num num1, num num2);

void* map_find(map* map, void* key);

int evaluate(evaluator* ev, expr* exp, expr* out);

expr* expr_new();

void* vector_get(vector* vec, unsigned long i);

expr* ev_unqualified_access(evaluator* ev, unsigned int x);

int is_literal(expr* exp);
