// Automatically generated header.

#pragma once
#include <stdlib.h>

#include "../corecommon/src/vector.h"
#include "../corecommon/src/hashtable.h"
#include "../corecommon/src/util.h"
typedef enum {
	move_left, move_right, move_inner,
	move_for_i, move_for_base, move_for_step,
	move_call_i
} move_kind;
typedef struct {
	struct value* to;
	char static_; //whether it can be inlined / passes all conditions statically
	vector_t val; //expression for every substitute indexes
} substitution;
typedef enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
	exp_cond, exp_def, exp_for, exp_call //2-3 args
} kind;
#include "numbers.h"
#include "lexer.h"
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
			vector_t sub; // multiple dispatch, iterate until condition checks
		} call;
	};
} expr;
typedef struct exp_idx {
	struct exp_idx* from;
	move_kind kind;
	unsigned long i; //index of value
	unsigned long i2; //index of substitute
} exp_idx;
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
	vector_t condition;
} sub_group;
typedef struct value {
	span s;
	vector_t groups; //conditions for substitutes in each expression
	vector_t substitutes; //vector_t of sub_idx specifying substitutes

	map_t substitute_idx;

	struct expr* exp;
} value;
typedef struct id {
	span s;
	char* name;
	vector_t val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
} id;
expr* expr_new();
void call_new(expr* exp, id* i);
int is_literal(expr* exp);
int is_value(expr* exp);
int is_binary(expr* exp);
int unary(expr* exp);
int def(expr* exp);
;

//copies expression by value
expr exp_copy_value(expr* exp);
expr* exp_copy(expr* exp);
void exp_rename(expr* exp, unsigned threshold, unsigned offset);
void gen_condition(value* val, expr* bind_exp, unsigned int i);
int condition(substitution* sub, unsigned long i, expr* root);
int static_condition(substitution* sub, unsigned long i, expr* root);
void gen_substitutes(value* val, expr* exp, unsigned int i);
expr* get_sub(substitution* sub, unsigned long i);
int substitute(expr* exp, substitution* sub);
binary_iterator binary_iter(expr* exp);
int binary_next(binary_iterator* iter);
expr* extract_operand(expr* exp, unsigned long x);
int remove_num(expr** eref, num* num);
void set_num(expr* e, num n);
void print_expr(expr* exp);
void exp_idx_free(exp_idx* idx);
void expr_free(expr* exp);
