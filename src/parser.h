// Automatically generated header.

#pragma once
#include "../corecommon/src/vector.h"
#include "../corecommon/src/hashtable.h"
#include "../corecommon/src/util.h"
#include "frontend.h"
#include "lexer.h"
typedef struct {
	frontend* fe;
	token current;

	unsigned long pos;

	module* mod;
	map_t* substitute_idx;
	vector_t reducers;
} parser;
