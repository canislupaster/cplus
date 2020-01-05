/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

typedef struct expr expr;
typedef struct span span;
typedef struct module module;
struct module {
	char* name;

	span s;
	vector tokens;

	map ids;
};
struct span {
	module* mod;

	char* start;
	char* end;
};
typedef struct id id;
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
struct id {
	span s;
	char* name;
	vector val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
};

void evaluate_main(frontend* fe);

extern char* MAIN;

num num_pow(num num1, num num2);

num num_div(num num1, num num2);

num num_mul(num num1, num num2);

num num_add(num num1, num num2);

void set_num(expr* e, num n);

int binary(expr* exp);

num num_invert(num n);

num* num_new(num x);

int unary(expr* exp);

int is_value(expr* exp);

void exp_rename(expr* exp, unsigned threshold, unsigned offset);

expr exp_copy_value(expr* exp);

expr* get_sub(substitution* sub, unsigned long i);

void* vector_pushcpy(vector* vec, void* x);

int vector_next(vector_iterator* iter);

vector_iterator vector_iterate(vector* vec);

extern num ZERO;

int num_eq(num num1, num num2);

int map_remove(map* map, void* key);

map_insert_result map_insertcpy(map* map, void* key, void* v);

int throw(const span* s, const char* x);

enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
			exp_cond, exp_def, exp_for, exp_call //2-3 args
};
typedef enum kind kind;

int def(expr* exp);

int condition(substitution* sub, unsigned long i, expr* root);

int ev_condition(evaluator* ev, substitution* sub, unsigned long i);

void* map_find(map* map, void* key);

int evaluate(evaluator* ev, expr* exp, expr* out);

expr* expr_new();

int is_literal(expr* exp);

void* vector_get(vector* vec, unsigned long i);

expr* ev_unqualified_access(evaluator* ev, unsigned int x);
