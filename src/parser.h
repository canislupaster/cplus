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

int map_next(map_iterator* iterator);
map_iterator map_iterate(map *map);
void print_module(module *b);

void parse(frontend* fe);

int parse_mod(parser* p, module* b);

typedef struct value value;
struct value {
	span s;
	vector groups; //conditions for substitutes in each expression
	vector substitutes; //vector of sub_idx specifying substitutes

	map substitute_idx;

	struct expr* exp;
};

void gen_substitutes(value* val, expr* exp, unsigned int i);

void gen_condition(value* val, expr* bind_exp, unsigned int i);

void reduce(expr** exp);

void map_configure_string_key(map* map, unsigned long size);

map map_new();

typedef struct sub_idx sub_idx;
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
struct sub_idx {
	unsigned int i;
	exp_idx* idx;
};
typedef struct sub_group sub_group;
struct sub_group {
	vector condition;
};

int parse_id(parser* p);

int substitute(expr* exp, substitution* sub);

void exp_rename(expr* exp, unsigned threshold, unsigned offset);

int cost(expr* exp);

extern const int CALL_COST;

char* isprintf(const char* fmt, ...);

void vector_free(vector* vec);

int static_condition(substitution* sub, unsigned long i, expr* root);

vector vector_new(unsigned long size);

void* vector_push(vector* vec);

extern num ONE;

expr* expr_new();

int binary(expr* exp);

void note(const span* s, const char* x);

struct id {
	span s;
	char* name;
	vector val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
};

void call_new(expr* exp, id* i);

expr* exp_copy(expr* exp);

int map_remove(map* map, void* key);

int vector_pop(vector* vec);

map_insert_result map_insertcpy(map* map, void* key, void* v);

void* vector_pushcpy(vector* vec, void* x);

void expr_free(expr* exp);

void drop(void* ptr);

expr* parse_expr(parser* p, int do_bind, unsigned op_prec);

expr* expr_new_p(parser* p, expr* first);

expr* parse_left_expr(parser* p, int bind);

typedef struct reducer reducer;
struct reducer {
	char* name;
	unsigned long x;
};

int vector_next(vector_iterator* iter);

vector_iterator vector_iterate(vector* vec);

unsigned long* parse_unqualified_access(parser* p, char* x);

void* map_find(map* map, void* key);

id* id_access(parser* p, name* x);

int try_parse_unqualified(parser* p);

int is_name(token* x);

int try_parse_name(parser* p);

void synchronize(parser* p);

int separator(parser* p);

int parse_next_eq(parser* p, token_type tt);

int parse_sync(parser* p);

int peek_sync(parser* p);

void parse_next(parser* p);

token* parse_peek(parser* p);

void* vector_get(vector* vec, unsigned long i);

token* parse_peek_x(parser* p, int x);

int throw(const span* s, const char* x);

int throw_here(parser* p, const char* x);
