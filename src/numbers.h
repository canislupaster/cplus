// Automatically generated header.

#pragma once
#include <stdint.h>

#include <math.h>

#include "../corecommon/src/util.h"
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
num* num_new(num x);
extern num ZERO;
extern num ONE;
int num_eq(num num1, num num2);
num num_mul(num num1, num num2);
num num_div(num num1, num num2);
num num_pow(num num1, num num2);
num num_add(num num1, num num2);
num num_invert(num n);
void print_num(num* n);
