/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

struct span;
struct module;
typedef struct {
	char* qualifier;
	char* x;
} name;
typedef struct span span;
struct module {
	char* name;

	span s;
	vector tokens;

	map ids;
};
typedef struct module module;
struct span {
	module* mod;

	char* start;
	char* end;
};
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
	unsigned long size;

	unsigned long length;
	char* data;
} vector;

vector vector_new(unsigned long size);

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
	module current;

	char errored; //whether to continue into next stage (ex. interpreter/codegen)

	map allocations; //ptr to trace
} frontend;

void evaluate_main(frontend* fe);

void parse(frontend* fe);

void lex(frontend* fe);

void frontend_free(frontend* fe);

int span_eq(span s, char* x);

#if _WIN32
void set_col(FILE *f,char color);
#endif

void module_init(module* b);

frontend make_frontend();
