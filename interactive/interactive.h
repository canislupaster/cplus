/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

typedef struct {
	unsigned long size;

	unsigned long length;
	char* data;
} vector;

void vector_clear(vector* vec);

typedef struct {
	char* start;
	char* end;
} span;
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

void evaluate_main(frontend* fe);

void parse(frontend* fe);

void lex(frontend* fe);

void frontend_free(frontend* fe);

int span_eq(span s, char* x);

#if _WIN32
void set_col(FILE *f,char color);
#endif
#if !(_WIN32)

void set_col(FILE* f, char color);

#endif

frontend make_frontend();
