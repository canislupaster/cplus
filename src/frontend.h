/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void memcheck();
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
typedef struct module module;
typedef struct {
	char* qualifier;
	char* x;
}name;
typedef struct span span;
struct span {
	module* mod;

	char* start;
	char* end;
};
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
typedef struct {
	module current;

	char errored; //whether to continue into next stage (ex. interpreter/codegen)

	map allocations; //ptr to trace
}frontend;
void frontend_free(frontend *fe);
int map_next(map_iterator *iterator);
map_iterator map_iterate(map *map);
typedef enum {
	t_name, t_non_bind,
	t_add, t_sub,
	t_ellipsis, t_comma,
	t_in, t_for,
	t_eq, t_lparen, t_rparen,
	t_str, t_num,
	t_sync, t_eof
}token_type;
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
typedef struct {
	token_type tt;
	span s;

	union {
		name* name;
		char* str;
		num* num;
	} val;
}token;
void token_free(token *t);
void module_free(module *b);
typedef struct id id;
struct id {
	span s;
	char* name;
	vector val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
};
void id_free(id *xid);
void map_free(map *map);
typedef struct expr expr;
int cost(expr *exp);
enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
			exp_cond, exp_def, exp_for, exp_call //2-3 args
};
typedef enum kind kind;
int binary(expr *exp);
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
void expr_free(expr *exp);
typedef struct exp_idx exp_idx;
typedef enum {
	move_left, move_right, move_inner,
	move_for_i, move_for_base, move_for_step,
	move_call_i
}move_kind;
struct exp_idx {
	struct exp_idx* from;
	move_kind kind;
	unsigned long i; //index of value
	unsigned long i2; //index of substitute
};
typedef struct {
	exp_idx* idx;

	expr* exp; //make sure it is equivalent to an expression at leaf-binding
	kind kind; //otherwise check kind and descend through substitute indexes
}sub_cond;
typedef struct sub_group sub_group;
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
}substitution;
int condition(substitution *sub,unsigned long i,expr *root);
struct sub_group {
	vector condition;
};
void vector_free(vector *vec);
typedef struct sub_idx sub_idx;
struct sub_idx {
	unsigned int i;
	exp_idx* idx;
};
void exp_idx_free(exp_idx *idx);
typedef struct {
	vector* vec;

	unsigned long i;
	char rev;
	void* x;
}vector_iterator;
int vector_next(vector_iterator *iter);
vector_iterator vector_iterate(vector *vec);
void value_free(value *val);
void map_configure_ptr_key(map *map,unsigned long size);
frontend make_frontend();
void map_configure_string_key(map *map,unsigned long size);
map map_new();
void module_init(module *b);
int read_file(module *mod,char *filename);
int is_name(token *x);
void print_num(num *n);
void note(const span *s,const char *x);
void warn(const span *s,const char *x);
int throw(const span *s,const char *x);
void drop(void *ptr);
void *vector_get(vector *vec,unsigned long i);
void *vector_pushcpy(vector *vec,void *x);
vector vector_new(unsigned long size);
#if _WIN32
void set_col(FILE *f,char color);
#endif
#if !(_WIN32)
void set_col(FILE *f,char color);
#endif
void msg(frontend *fe,const span *s,char color1,char color2,const char *template_empty,const char *template,const char *msg);
void *heap(size_t size);
char *spanstr(span *s);
unsigned long span_len(span *s);
int span_eq(span s,char *x);
int compare_name(name *x1,name *x2);
uint64_t hash_string(char **x);
uint64_t hash_name(name *x);
extern const span SPAN_NULL;
extern frontend *FRONTEND;
