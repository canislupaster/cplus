// Automatically generated header.

#pragma once
#include <stdlib.h>

#include <stdint.h>

typedef struct module module;
typedef struct span {
	module* mod;

	char* start;
	char* end;
} span;
typedef struct {
	char* qualifier;
	char* x;
} name;
typedef enum {
	t_name, t_non_bind,
	t_add, t_sub,
	t_ellipsis, t_comma,
	t_in, t_for,
	t_eq, t_lparen, t_rparen,
	t_str, t_num,
	t_sync, t_eof
} token_type;
#include "numbers.h"
typedef struct {
	token_type tt;
	span s;

	union {
		name* name;
		char* str;
		num* num;
	} val;
} token;
int is_name(token* x);
void token_free(token* t);
