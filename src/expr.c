#import "expr.h"

expr* expr_new(expr* first) {
  expr* x = heap(sizeof(expr));

  x->cost = 0;
  x->first = first;

  x->s.start = NULL;
  x->s.end = NULL;
  return x;
}

expr* expr_new_p(parser* p, expr* first) {
  expr* x = expr_new(first);
  x->s.start = x->first ? x->first->s.start : p->current.s.start;

  return x;
}

int is_value(expr* exp) {
  return !exp->first && exp->kind == exp_add;
}

int binary(expr* exp) {
  return exp->kind != exp_for && exp->kind != exp_invert && exp->kind != exp_call;
}

expr* exp_copy(expr* exp) {
  expr* new_exp = heapcpy(sizeof(expr), exp);
  *new_exp = *exp;

  if (exp->first)
	new_exp->first = exp_copy(exp->first);

  switch (new_exp->kind) {
  case exp_cond:
  case exp_def:
  case exp_for: {
	new_exp->_for.i = exp_copy(new_exp->_for.i);
	new_exp->_for.base = exp_copy(new_exp->_for.base);

	break;
  }
  case exp_call: {
	vector_cpy(&exp->call.sub.condition, &new_exp->call.sub.condition);
	vector_cpy(&exp->call.sub.val, &new_exp->call.sub.val);

	vector_iterator iter = vector_iterate(&new_exp->call.sub.val);
	while (vector_next(&iter)) {
	  iter.x = exp_copy(*(expr**)iter.x);
	}
  }

  default:;
  }

  return new_exp;
}

//uniquely renames identifiers in loops n stuff
void exp_rename(expr* exp, unsigned offset) {
  if (binary(exp) && exp->ty == exp_bind) {
	exp->val.bind += offset;
  } else if (exp->kind == exp_for) {
	exp->_for.i += offset;
  }
}

exp_idx* descend(exp_idx* start, move_kind kind) {
  exp_idx* new = heap(sizeof(exp_idx));
  new->from = start;
  new->kind = kind;

  return new;
}

exp_idx* descend_i(exp_idx* start, move_kind kind, unsigned long i) {
  exp_idx* new = descend(start, kind);
  new->i = i;

  return new;
}

expr* goto_idx(expr* root, exp_idx* where) {
  if (!where)
	return root;

  root = goto_idx(root, where->from);

  switch (where->kind) {
  case move_left: return root->first;
  case move_right: {
	if (root->ty == exp_inner)
	  return root->val.inner;
	else { //no inner expression, create new and set value to right of root
	  //FIXME: probably leaks memory
	  expr* new_exp = expr_new(NULL);
	  new_exp->kind = exp_add;
	  new_exp->ty = root->ty;
	  new_exp->val = root->val;

	  return new_exp;
	}
  }

  case move_for_base: return root->_for.base;
  case move_for_i: return root->_for.i;

  case move_call_i: return vector_get(&root->call.sub.val, where->i);
  }
}

int bind(expr* from, expr* to, substitution* sub, exp_idx* cursor) {
  if (from->first && to->first) {
	bind(from->first, to->first, sub, descend(cursor, move_left));
  }

  if (binary(from) && from->ty == exp_bind) { //make sure from == to at runtime
	vector_pushcpy(&sub->condition,
				   &(sub_cond){.where = cursor, .x = from->val.bind});
  } else if (binary(from) && binary(to)
	  && from->ty == to->ty) {

	switch (from->ty) {
	case exp_num: return num_eq(*from->val.by, *to->val.by);
	case exp_inner: return bind(from->val.inner, to->val.inner, sub, descend(cursor, move_right));
	default:;
	}
  } else if (is_value(to)) {
	if (to->ty == exp_bind)
	  vector_pushcpy(&sub->val, &from);
	else {
	  vector_pushcpy(&sub->condition,
					 &(sub_cond){.where = cursor, .x = from->val.bind});
	}
  } else {
	return 0;
  }

  if (to->kind != from->kind)
	return 1;

  switch (from->kind) {
  case exp_call: {
	if (from->call.to != to->call.to)
	  return 0;

	vector_iterator iter = vector_iterate(&from->call.sub.val);
	while (vector_next(&iter)) {
	  expr* exp2 = vector_get(&to->call.sub.val, iter.i - 1);
	  if (!exp2)
		return 0;

	  if (!bind(iter.x, exp2, sub, descend_i(cursor, move_call_i, iter.i - 1)))
		return 0;
	}
  }

  case exp_cond:
  case exp_def:
  case exp_for: {
	//x should always be the same, match on base and i
	if (!bind(from->_for.base, to->_for.base, sub, descend(cursor, move_for_base)))
	  return 0;
	if (!bind(from->_for.i, to->_for.i, sub, descend(cursor, move_for_i)))
	  return 0;

	break;
  }

  case exp_invert: break;
  default:;
  }

  return 1;
}

//substitutes in reverse in comparison to bind function
int substitute(expr* exp, substitution* sub) {
  switch (exp->kind) {
  case exp_invert: break;

  case exp_cond:
  case exp_def:
  case exp_for: {
	substitute(exp->_for.base, sub);
	substitute(exp->_for.i, sub);

	break;
  }
  case exp_call: {
	vector_iterator iter = vector_iterate(&exp->call.sub.val);
	iter.rev = 1;

	while (vector_next(&iter)) {
	  substitute(*(expr**)iter.x, sub);
	}

	break;
  }

  default: {
	if (exp->ty == exp_bind) {
	  expr** res = vector_get(&sub->val, exp->val.bind);
	  if (res) { //if binding is a substitute
		exp->ty = exp_inner;
		exp->val.inner = *(expr**)res;
	  }
	} else if (exp->ty == exp_inner) {
	  substitute(exp->val.inner, sub);
	}
  }
  }

  if (exp->first)
	substitute(exp->first, sub);

  return 1;
}

int binding_exists(expr* exp, unsigned long x) {
  int amount = 0;

  if (binary(exp)) {
	if (exp->ty == exp_bind && exp->val.bind == x)
	  amount++;
	else if (exp->ty == exp_inner)
	  amount += binding_exists(exp->val.inner, x);
  }

  if (exp->first)
	return amount + binding_exists(exp->first, x);
  else
	return amount;
}

//extracts operand on other side (than x1) of operator
expr* extract_operand(expr* exp, unsigned long x1) {
  if (binding_exists(exp, x1) != 1)
	return NULL;

  if (binary(exp) && exp->ty == exp_bind && exp->val.bind == x1) {
	expr_head_free(exp);
	return exp->first;
  }

  //only descend one layer max
  if (exp->first && is_value(exp->first)) {
	expr_free(exp->first);
	exp->first = NULL;
	exp->kind = exp_add;
	return exp;
  }

  return NULL;
}

int remove_num(expr** eref, num* num) {
  expr* exp = *eref;
  //only descend one layer max
  if (exp->first && is_value(exp->first)) {
	if (exp->first->ty == exp_num && num_eq(*exp->first->val.by, *num)) {
	  expr_free(exp->first);
	  exp->first = NULL;
	  exp->kind = exp_add;

	  return 1;
	}
  }

  if (binary(exp) && exp->ty == exp_num && num_eq(*exp->val.by, *num)) {
	*eref = exp->first;
	expr_head_free(exp);
  }

  return 0;
}

void print_expr(expr* exp) {
  if (exp->kind == exp_invert) {
	printf("-");
  }

  if (exp->first) {
	printf("(");
	print_expr(exp->first);
	printf(")");
  }

  switch (exp->kind) {
  case exp_invert: break;

  case exp_add: printf("+");
	break;
  case exp_div: printf("/");
	break;
  case exp_mul: printf("*");
	break;
  case exp_pow: printf("^");
	break;

  case exp_for: {
	printf(" for (x=@%lu) from ", exp->_for.x);
	print_expr(exp->_for.base);
	printf(", ");
	print_expr(exp->_for.i);
	printf(" times");

	break;
  }

  case exp_cond: {
	printf(" if (x=@%lu) ", exp->_for.x);
	print_expr(exp->_for.i);
	printf(" else ");
	print_expr(exp->_for.base);

	break;
  }

  case exp_def: {
	printf("where @%lu = ", exp->_for.x);
	print_expr(exp->_for.base);

	break;
  }

  case exp_call: {
	printf("call ");

	vector_iterator iter = vector_iterate(&exp->call.sub.val);
	while (vector_next(&iter)) {
	  print_expr(*(expr**)iter.x);
	  printf(" ");
	}

	break;
  }
  }

  if (binary(exp)) {
	switch (exp->ty) {
	case exp_num: print_num((num*)exp->val.by);
	  break;
	  //        case : printf("\"%s\"", (char*)e->val.str); break;
	case exp_inner: {
	  printf("(");
	  print_expr((expr*)exp->val.inner);
	  printf(")");
	  break;
	}
	case exp_bind: printf("@%lu", exp->val.bind);
	  break;
	}
  }
}

void expr_head_free(expr* exp) {
  switch (exp->kind) {
  case exp_for: {
	expr_free(exp->_for.base);
	expr_free(exp->_for.i);

	break;
  }
  case exp_call: {
	vector_iterator iter = vector_iterate(&exp->call.sub.val);
	while (vector_next(&iter)) {
	  expr_free(*(expr**)iter.x);
	}

	vector_free(&exp->call.sub.val);

	break;
  }
  default:;
  }

  free(exp);
}

void expr_free(expr* exp) {
  if (exp->first)
	expr_free(exp->first);
  expr_head_free(exp);
}