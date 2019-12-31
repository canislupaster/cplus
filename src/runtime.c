#include "runtime.h"

expr ev_unqualified_access(evaluator* ev, unsigned int x) {
  expr* sub = vector_get(&ev->sub->val, x);
  if (sub)
	return *sub;

  expr* val = map_find(&ev->scope, &x);
  return *val;
}

// extract value from other operand
expr rhs(evaluator* ev, expr* exp) {
  switch (exp->ty) {
  case exp_inner: {
	expr new_exp;
	evaluate(ev, exp->val.inner, &new_exp);

	return new_exp;
  }
  case exp_num: return *exp;
  case exp_bind: return ev_unqualified_access(ev, exp->val.bind);
  }
}

//checks equality of values
int condition(evaluator* ev, expr* from, expr* to) {
  if (to->kind != from->kind)
	return 0;

  switch (from->kind) {
  case exp_call: {
	if (from->call.to != to->call.to)
	  return 0;

	vector_iterator iter = vector_iterate(&from->call.sub.val);
	vector_iterator iter2 = vector_iterate(&to->call.sub.val);
	while (1) {
	  int res1 = vector_next(&iter);
	  int res2 = vector_next(&iter2);

	  if (res1 != res2)
		return 0;
	  else if (res1 == 0)
		return 1;

	  expr exp1, exp2;

	  if (!evaluate(ev, iter.x, &exp1) || !evaluate(ev, iter2.x, &exp2))
		return 0;
	  if (!condition(iter.x, &exp1, &exp2))
		return 0;
	}
  }

  default: {
	if (from->ty == to->ty) {
	  switch (from->ty) {
	  case exp_num: return num_eq(*from->val.by, *to->val.by);
	  default: return 0;
	  }
	} else {
	  return 0;
	}
  }
  }
}

//unwrap for loops and compute arithmetic
int evaluate(evaluator* ev, expr* exp, expr* out) {
  if (exp->kind == exp_for) {
	expr i;
	if (!evaluate(ev, exp->_for.i, &i))
	  return 0;
	if (i.ty != exp_num)
	  return throw(&exp->_for.i->s, "expected number for loop");
	if (i.val.by->ty == num_decimal)
	  return throw(&exp->_for.i->s, "expected integer for loop iterator");

	expr* state = exp->_for.base;
	map_insertcpy(&ev->scope, &exp->_for.x, &state);

	uint64_t int_i = i.val.by->uint;
	for (uint64_t iter = 0; iter < int_i - 1; iter++) {
	  evaluate(ev, exp->first, state);

	  map_insertcpy(&ev->scope, &exp->_for.x, &state);
	}

	evaluate(ev, exp->first, out);

	map_remove(&ev->scope, &exp->_for.x);
  }

  expr first;
  if (exp->first)
	if (!evaluate(ev, exp->first, &first))
	  return 0;
  expr erhs = rhs(ev, exp);

  //check left
  if (binary(exp) || exp->kind == exp_invert) {
	if (first.ty != exp_num)
	  return throw(&exp->first->s, "operand is not a number, cannot be used in numerical operations");
  }

  if (binary(exp)) {
	num num1 = *first.val.by;
	num num2 = *erhs.val.by;

	//check right
	if (erhs.ty != exp_num)
	  return throw(&erhs.s, "right hand side is not a number, cannot be used in numerical operations");

	out->ty = exp_num;

	switch (exp->kind) {
	case exp_add: out->val.by = num_new(num_add(num1, num2));
	  break;
	case exp_mul: out->val.by = num_new(num_mul(num1, num2));
	  break;
	case exp_div: out->val.by = num_new(num_div(num1, num2));
	  break;
	case exp_pow: out->val.by = num_new(num_pow(num1, num2));
	  break;
	}
  } else {
	switch (exp->kind) {
	case exp_invert: {
	  out->ty = exp_num;
	  out->val.by = num_new(num_invert(*first.val.by));
	}
	case exp_call: {
	  ev->sub = &exp->call.sub;

	  vector_iterator iter = vector_iterate(&exp->call.sub.condition);
	  while (vector_next(&iter)) {
		sub_cond* cond = iter.x;
		condition(ev,)
	  }

	  out = evaluate()
	  ev->sub = NULL;
	}
	case exp_def:
	case exp_cond:
	}
  }

  out->kind = exp_add;
  out->first = NULL;

  return 1;
}

void evaluate_main(frontend* fe) {
  id* main = map_find(&fe->global.ids, &"main");

  if (main->val.substitutes.length > 0) {
	throw(&main->s, "there must be no substitutes of main");
	return;
  }

  evaluator ev = {.mod=&fe->global, .fe=fe, .scope=map_new(), .sub=NULL};
  map_configure_ulong_key(&ev.scope, sizeof(expr*));

  expr out;
  evaluate(&ev, main->val.val, &out);
}