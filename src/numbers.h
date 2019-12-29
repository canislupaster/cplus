/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
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
num invert(num n);
num add(num num1,num num2);
num num_pow(num num1,num num2);
num num_div(num num1,num num2);
num mul(num num1,num num2);
int num_gt(num num1,num than);
int num_eq(num num1,num num2);
extern num ONE;
extern num ZERO;
void commute(num *num1,num *num2);
void convert_dec(num *n);
