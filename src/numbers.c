#include "numbers.h"

void convert_dec(num* n) {
  if (n->ty != num_decimal) {
	n->decimal = (long double)n->uint;
	n->ty = num_decimal;
  }
}

void commute(num* num1, num* num2) {
  if (num1->ty != num2->ty) { //ensure same precision, assume two's complement
	convert_dec(num1);
	convert_dec(num2);
  }
}

num ZERO = {.ty = num_integer, .integer = 0};
num ONE = {.ty = num_integer, .integer = 1};

int num_eq(num num1, num num2) {
  commute(&num1, &num2);

  if (num1.ty == num_decimal) return num1.decimal == num2.decimal;
  else return num1.uint == num2.uint;
}

int num_gt(num num1, num than) {
  commute(&num1, &than);

  if (num1.ty == num_decimal) return num1.decimal > than.decimal;
  else return num1.uint > than.uint;
}

num mul(num num1, num num2) {
  commute(&num1, &num2);

  num res = {.ty = num1.ty};
  if (num1.ty == num_decimal) {
	res.decimal = num1.decimal * num2.decimal;
  } else {
	res.uint = num1.uint * num2.uint;
  }

  return res;
}

num num_div(num num1, num num2) {
  convert_dec(&num1);
  convert_dec(&num2);

  num res = {.ty = num_decimal};
  if (num1.ty == num_decimal) {
	res.decimal = num1.decimal / num2.decimal;
  }

  return res;
}

num num_pow(num num1, num num2) {
  commute(&num1, &num2);

  num res = {.ty = num1.ty};
  if (num1.ty == num_decimal) {
	res.decimal = powl(num1.decimal, num2.decimal);
  } else {
	res.uint = num1.uint ^ num2.uint;
  }

  return res;
}

num add(num num1, num num2) {
  commute(&num1, &num2);

  num res = {.ty = num1.ty};
  if (num1.ty == num_decimal) {
	res.decimal = num1.decimal + num2.decimal;
  } else {
	res.uint = num1.uint + num2.uint;
  }

  return res;
}

num invert(num n) {
  num res = n;
  if (res.ty == num_decimal) res.decimal = -res.decimal;
  else res.integer = -res.integer;

  return res;
}

void set_num(expr* e, num n) {
  e->kind = exp_add;
  e->ty = exp_num;
  e->val.by = num_new(n);
}