/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

typedef struct module module;
typedef struct span span;
struct span {
	module* mod;

	char* start;
	char* end;
};
struct module {
	char* name;

	span s;
	vector tokens;

	map ids;
};

void frontend_free(frontend* fe);

void token_free(token* t);

void module_free(module* b);

typedef struct id id;
struct id {
	span s;
	char* name;
	vector val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
};

void id_free(id* xid);

typedef struct expr expr;
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
typedef struct sub_group sub_group;
struct sub_group {
	vector condition;
};
typedef struct value value;

int condition(substitution* sub, unsigned long i, expr* root);

void vector_free(vector* vec);

typedef struct sub_idx sub_idx;
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

void exp_idx_free(exp_idx* idx);

int vector_next(vector_iterator* iter);

vector_iterator vector_iterate(vector* vec);

struct value {
	span s;
	vector groups; //conditions for substitutes in each expression
	vector substitutes; //vector of sub_idx specifying substitutes

	map substitute_idx;

	struct expr* exp;
};

void value_free(value* val);

void map_configure_ptr_key(map* map, unsigned long size);

frontend make_frontend();

void map_configure_string_key(map* map, unsigned long size);

map map_new();

void module_init(module* b);

int read_file(module* mod, char* filename);

int is_name(token* x);

void print_num(num* n);

void map_free(map* map);

int map_next(map_iterator* iterator);

map_iterator map_iterate(map* map);

void memcheck();

int map_remove(map* map, void* key);

void* resize(void* ptr, size_t size);

void* heapcpy(size_t size, const void* val);

map_insert_result map_insertcpy(map* map, void* key, void* v);

void print_trace(trace* trace);

#define TRACE_SIZE 10

trace stacktrace();

void note(const span* s, const char* x);

void warn(const span* s, const char* x);

int throw(const span* s, const char* x);

void drop(void* ptr);

void* vector_get(vector* vec, unsigned long i);

void* vector_pushcpy(vector* vec, void* x);

vector vector_new(unsigned long size);

#if _WIN32
void set_col(FILE *f,char color);
#endif

void msg(frontend* fe,
				 const span* s,
				 char color1,
				 char color2,
				 const char* template_empty,
				 const char* template,
				 const char* msg);

void* heap(size_t size);

char* spanstr(span* s);

unsigned long span_len(span* s);

int span_eq(span s, char* x);

int compare_name(name* x1, name* x2);

uint64_t hash_string(char** x);

uint64_t hash_name(name* x);

extern const span SPAN_NULL;
extern frontend* FRONTEND;
