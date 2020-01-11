/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
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
num *num_new(num x);
enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
			exp_cond, exp_def, exp_for, exp_call //2-3 args
};
typedef enum kind kind;
typedef struct expr expr;
typedef struct span span;
typedef struct module module;
typedef struct {
	char* qualifier;
	char* x;
}name;
typedef struct {
	unsigned long size;

	unsigned long length;
	char* data;
}vector;
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
int cost(expr *exp);
int binary(expr *exp);
typedef struct id id;
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
void set_num(expr *e,num n);
num num_invert(num n);
num num_add(num num1,num num2);
num num_pow(num num1,num num2);
num num_div(num num1,num num2);
num num_mul(num num1,num num2);
int num_eq(num num1,num num2);
extern num ONE;
extern num ZERO;
void commute(num *num1,num *num2);
void convert_dec(num *n);
