/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

void map_free(map* map);

void map_configure_ulong_key(map* map, unsigned long size);

map map_new();

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

void reduce(expr** exp);

typedef struct value value;

int vector_next(vector_iterator* iter);

vector_iterator vector_iterate(vector* vec);

expr* extract_operand(expr* exp, unsigned long x);

void expr_free(expr* exp);

int num_eq(num num1, num num2);

void* map_find(map* map, void* key);

extern num ZERO;
extern num ONE;

int remove_num(expr** eref, num* num);

num num_pow(num num1, num num2);

num num_div(num num1, num num2);

num num_mul(num num1, num num2);

num num_add(num num1, num num2);

void set_num(expr* e, num n);

num num_invert(num n);

num* num_new(num x);

expr* expr_new();

void drop(void* ptr);

int binary_next(binary_iterator* iter);

binary_iterator binary_iter(expr* exp);

map_insert_result map_insert(map* map, void* key);

enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
			exp_cond, exp_def, exp_for, exp_call //2-3 args
};
typedef enum kind kind;

int binary(expr* exp);

int unary(expr* exp);

int cost(expr* exp);

extern const int CALL_COST;
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
