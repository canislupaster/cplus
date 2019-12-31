#import "optimizer.h"

const int CALL_COST = 10;

int cost(expr* e) {
  if (e->cost)
	return e->cost;
  int acc = e->first ? cost(e->first) : 0;

  switch (e->kind) {
  case exp_call: {
	vector_iterator iter = vector_iterate(&e->call.sub.val);
	while (vector_next(&iter)) {
	  acc += cost(iter.x);
	}

	e->cost = acc + CALL_COST;
  }

  case exp_for: e->cost = acc * cost(e->_for.i) + cost(e->_for.base);
	break;
  case exp_cond: e->cost = acc + cost(e->_for.i) + cost(e->_for.base);
	break;
  case exp_def: e->cost = acc + cost(e->_for.base);
	break;

  default: e->cost = 1;
  }

  return e->cost;
}

// simple linear optimizer
typedef struct {
  map usages;
} optimizer;

//bottom up optimizer
static void opt_reduce(expr** eref, optimizer* opt) {
  expr* e = *eref;

  //redundant expression
  if (is_value(e) && binary(e) && e->ty == exp_inner) {
	e = e->val.inner;
  }

  if (e->first)
	opt_reduce(&e->first, opt);

  if (binary(e) && e->ty == exp_inner) {
	opt_reduce(&e->val.inner, opt);
  } else if (binary(e) && e->ty == exp_bind) {
	map_insert(&opt->usages, &e->val.bind);
  }

  if (e->kind == exp_invert && e->first && e->first->kind == exp_add
	  && e->first->first && e->first->first->kind == exp_invert) {
	expr* first = e->first->first->first;
	expr* amount = e->first;
	free(e);

	e = expr_new(first);
	e->kind = exp_add;
	e->first = first;

	//cleanly invert num / inner / bind
	switch (amount->ty) {
	case exp_num: {
	  e->ty = exp_num;
	  e->val.by = num_new(num_invert(*amount->val.by));
	  free(amount);
	  break;
	}
	case exp_inner: {
	  e->ty = exp_inner;
	  e->val.inner = expr_new(amount->val.inner);
	  e->val.inner->kind = exp_invert;

	  free(amount);

	  break;
	}
	case exp_bind: {
	  e->ty = exp_inner;

	  amount->first = NULL;
	  e->val.inner = expr_new(amount);
	  e->val.inner->kind = exp_invert;
	}
	}
  }

  //inner expression is a constant, extract
  if (binary(e) && e->ty == exp_inner && binary(e->val.inner) && is_value(e->val.inner)) {
	expr* inner = e->val.inner;
	e->ty = inner->ty;
	e->val.by = inner->val.by;

	expr_free(inner);
  }

  if (e->first && is_value(e->first) && e->first->ty == exp_num && (!binary(e) || e->ty == exp_num)) {
	switch (e->kind) {
	case exp_add: set_num(e, num_add(*e->first->val.by, *e->val.by));
	  break;
	case exp_mul: set_num(e, num_mul(*e->first->val.by, *e->val.by));
	  break;
	case exp_div: set_num(e, num_div(*e->first->val.by, *e->val.by));
	  break;
	case exp_pow: set_num(e, num_pow(*e->first->val.by, *e->val.by));
	  break;
	case exp_invert: set_num(e, num_invert(*e->first->val.by));
	  break;

	default:;
	}
  }

  if (e->kind == exp_mul || e->kind == exp_div) {
	remove_num(eref, &ONE);
	e = *eref;
  } else if (e->kind == exp_add && e->first) {
	remove_num(eref, &ZERO);
	e = *eref;
  }

  //TODO: simplify to def if i is 1
  switch (e->kind) {
  case exp_for: {
	reduce(&e->_for.base);
	reduce(&e->_for.i);

	if (is_value(e->_for.i) && e->_for.i->ty == exp_num && num_eq(*e->_for.i->val.by, ONE)) {
	  if (!e->_for.named || !map_find(&opt->usages, &e->_for.x)) {
		expr_free(e->_for.base);
		expr* first = e->first;
		free(e);
		e = first; //i is one and x/base is not used, return step
	  }

	  e->kind = exp_def;
	} else if (!e->_for.named || !map_find(&opt->usages, &e->_for.x)) {
	  e->kind = exp_cond; //expression can be simplified to a conditional if step (first) does not use x
	} else if (e->first->kind == exp_add) {
	  //reduce repeated addition to multiplication
	  expr* by = extract_operand(e->first, e->_for.x);
	  if (!by)
		break;

	  free(e);
	  expr* i = e->_for.i;
	  expr* base = e->_for.base;

	  e = expr_new(by);
	  e->kind = exp_mul;
	  e->ty = exp_inner;
	  e->val.inner = i;

	  e = expr_new(e);
	  e->kind = exp_add;
	  e->ty = exp_inner;
	  e->val.inner = base;

	  *eref = e;

	  return opt_reduce(eref, opt);
	} else if (e->first->kind == exp_mul) {
	  //reduce repeated multiplication to exponentiation
	  expr* by = extract_operand(e->first, e->_for.x);
	  if (!by)
		break;

	  free(e);
	  expr* i = e->_for.i;
	  expr* base = e->_for.base;

	  e = expr_new(by);
	  e->kind = exp_pow;
	  e->ty = exp_inner;
	  e->val.inner = i;

	  e = expr_new(e);
	  e->kind = exp_mul;
	  e->ty = exp_inner;
	  e->val.inner = base;

	  *eref = e;

	  return opt_reduce(eref, opt);
	}

	break;
  }
  case exp_call: {
	vector_iterator iter = vector_iterate(&e->call.sub.val);
	while (vector_next(&iter)) {
	  opt_reduce(iter.x, opt);
	}

	break;
  }

  default:;
  }

  *eref = e;
}

void reduce(expr** exp) {
  optimizer opt = {.usages=map_new()};
  map_configure_ulong_key(&opt.usages, 0);
  opt_reduce(exp, &opt);
}