/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

struct module;
typedef struct {
	char* qualifier;
	char* x;
} name;
struct span;
typedef struct module module;
struct span {
	module* mod;

	char* start;
	char* end;
};
typedef struct span span;
struct module {
	char* name;

	span s;
	vector tokens;

	map ids;
};
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

void frontend_free(frontend* fe);

void evaluate_main(frontend* fe);

void print_module(module* b);

void parse(frontend* fe);

void lex(frontend* fe);

int read_file(module* mod, char* filename);

frontend make_frontend();
