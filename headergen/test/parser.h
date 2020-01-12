// Automatically generated header.

#include "hashtable.h"
#include "frontend.h"
#include "lexer.h"
#include "vector.h"
typedef struct {
	frontend* fe;
	token current;

	unsigned long pos;

	module* mod;
	map* substitute_idx;
	vector reducers;
} parser;
