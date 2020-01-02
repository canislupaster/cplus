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

#define CONTROL_BYTES 16
typedef struct {
	uint8_t control_bytes[CONTROL_BYTES];
} bucket;
typedef struct {
	map* map;

	char c;
	unsigned long bucket;

	void* key;
	void* x;
	char current_c;
	bucket* bucket_ref;
}map_iterator;
int map_next(map_iterator *iterator);
map_iterator map_iterate(map *map);
void print_module(module *b);
void parse(frontend *fe);

typedef enum {
	t_name, t_non_bind,
	t_add, t_sub,
	t_ellipsis, t_comma,
	t_in, t_for,
	t_eq, t_lparen, t_rparen,
	t_str, t_num,
	t_sync, t_eof
} token_type;
typedef struct {
	char* qualifier;
	char* x;
} name;
typedef struct {
	token_type tt;
	span s;

	union {
		name* name;
		char* str;
		num* num;
	} val;
} token;
typedef struct {
	frontend* fe;
	token current;

	unsigned long pos;

	module* mod;
	map* substitute_idx;
	vector reducers;
} parser;

int parse_mod(parser* p, module* b);

void reduce(expr** exp);

void map_configure_string_key(map* map, unsigned long size);

map map_new();

int parse_id(parser* p);

int substitute(expr* exp, substitution* sub);

void exp_rename(expr* exp, unsigned threshold, unsigned offset);

extern const int CALL_COST;

char* isprintf(const char* fmt, ...);

typedef struct {
	unsigned int x;
	struct expr* what;
} sub_cond;

vector vector_new(unsigned long size);

void note(const span* s, const char* x);

extern num ONE;

expr* expr_new();

expr* exp_copy(expr* exp);

int map_remove(map* map, void* key);

int vector_pop(vector* vec);

typedef struct {
	void* val;
	char exists;
} map_insert_result;

map_insert_result map_insertcpy(map* map, void* key, void* v);

void* vector_pushcpy(vector* vec, void* x);

void expr_free(expr* exp);

expr* parse_expr(parser* p, int do_bind, unsigned op_prec);

expr* expr_new_p(parser* p, expr* first);

expr* parse_left_expr(parser* p, int bind);

typedef struct reducer reducer;
struct reducer {
	char* name;
	unsigned long x;
};
typedef struct {
	vector* vec;

	unsigned long i;
	char rev;
	void* x;
} vector_iterator;

int vector_next(vector_iterator* iter);

vector_iterator vector_iterate(vector* vec);

unsigned long* parse_unqualified_access(parser* p, char* x);

void* map_find(map* map, void* key);

typedef struct id id;
struct id {
	span s;
	char* name;
	value val;
	span substitutes;
	unsigned precedence;
};

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
