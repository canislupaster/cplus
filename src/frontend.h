// Automatically generated header.

#pragma once
#include <stdio.h>

#include "../corecommon/src/vector.h"
#include "../corecommon/src/hashtable.h"
#include "../corecommon/src/util.h"
#include "colors.h"
#include "lexer.h"
typedef struct module {
	char* name;

	span s;
	vector_t tokens;

	map_t ids;
} module;
typedef struct {
	module current;

	char errored; //whether to continue into next stage (ex. interpreter/codegen)

	map_t allocations; //ptr to trace
} frontend;
int span_eq(span s, char* x);
unsigned long span_len(span* s);
char* spanstr(span* s);
int throw(const span* s, const char* x);
void note(const span* s, const char* x);
void module_init(module* b);
frontend make_frontend();
void frontend_free(frontend* fe);
