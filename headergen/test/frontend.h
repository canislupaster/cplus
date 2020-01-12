// Automatically generated header.

#include "lexer.h"
#include "vector.h"
typedef struct id {
	span s;
	char* name;
	vector val; //multiple dispatch of different substitute <-> exp
	unsigned precedence;
} id;
typedef struct reducer {
	char* name;
	unsigned long x;
} reducer;
typedef struct module {
	char* name;

	span s;
	vector tokens;

	map ids;
} module;
typedef struct {
	module current;

	char errored; //whether to continue into next stage (ex. interpreter/codegen)

	map allocations; //ptr to trace
} frontend;
int throw(const span* s, const char* x);
void note(const span* s, const char* x);
int is_name(token* x);
