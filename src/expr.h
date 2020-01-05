/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

void drop(void* ptr);

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

void vector_free(vector* vec);

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

void expr_free(expr* exp);

typedef struct exp_idx exp_idx;
enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
			exp_cond, exp_def, exp_for, exp_call //2-3 args
};
typedef enum kind kind;
struct exp_idx {
	struct exp_idx* from;
	move_kind kind;
	unsigned long i; //index of value
	unsigned long i2; //index of substitute
};

void exp_idx_free(exp_idx* idx);

void print_num(num* n);

void print_expr(expr* exp);

int remove_num(expr** eref, num* num);

expr* extract_operand(expr* exp, unsigned long x);

int binary_next(binary_iterator* iter);

binary_iterator binary_iter(expr* exp);

int binding_exists(expr* exp, unsigned long x);

typedef struct value value;

int substitute(expr* exp, substitution* sub);

expr* get_sub(substitution* sub, unsigned long i);

typedef struct sub_idx sub_idx;
struct sub_idx {
	unsigned int i;
	exp_idx* idx;
};
struct value {
	span s;
	vector groups; //conditions for substitutes in each expression
	vector substitutes; //vector of sub_idx specifying substitutes

	map substitute_idx;

	struct expr* exp;
};

void gen_substitutes(value* val, expr* exp, unsigned int i);

int static_condition(substitution* sub, unsigned long i, expr* root);

int num_eq(num num1, num num2);

int literal_eq(expr* exp1, expr* exp2);

int condition(substitution* sub, unsigned long i, expr* root);

void* vector_setcpy(vector* vec, unsigned long i, void* x);

typedef struct sub_group sub_group;
struct sub_group {
	vector condition;
};

void gen_condition(value* val, expr* bind_exp, unsigned int i);

void exp_rename(expr* exp, unsigned threshold, unsigned offset);

void vector_cpy(vector* from, vector* to);

expr* exp_copy(expr* exp);

expr exp_copy_value(expr* exp);

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

void* heapcpy(size_t size, const void* val);

exp_idx* exp_idx_copy(exp_idx* from);

int def(expr* exp);

int unary(expr* exp);

int binary(expr* exp);

int is_value(expr* exp);

int vector_next(vector_iterator* iter);

vector_iterator vector_iterate(vector* vec);

int is_literal(expr* exp);

vector vector_new(unsigned long size);

struct id {
	span s;
	char* name;
	vector val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
};

void call_new(expr* exp, id* i);

expr* expr_new_p(parser* p, expr* first);

int cost(expr* exp);

void* heap(size_t size);

expr* expr_new();
