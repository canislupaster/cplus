/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
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

map map_new();

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

void reduce(expr** exp);

typedef struct value value;
struct value {
	span s;
	vector groups; //conditions for substitutes in each expression
	vector substitutes; //vector of sub_idx specifying substitutes

	map substitute_idx;

	struct expr* exp;
};
typedef struct {
	struct value* to;
	char static_; //whether it can be inlined / passes all conditions statically
	vector val; //expression for every substitute indexes
} substitution;
typedef struct {
	vector* vec;

	unsigned long i;
	char rev;
	void* x;
} vector_iterator;

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

typedef struct {
	expr* exp;
	expr* x;
	expr* other;

	char right;
} binary_iterator;

int binary_next(binary_iterator* iter);

binary_iterator binary_iter(expr* exp);

typedef struct {
	void* val;
	char exists;
} map_insert_result;

map_insert_result map_insert(map* map, void* key);

int unary(expr* exp);

extern const int CALL_COST;
