#include "parser.h"

int throw_here(parser* p, const char* x) {
  return throw(&p->current.s, x);
}

token* parse_peek_x(parser* p, int x) {
  token* tok = vector_get(&p->fe->tokens, p->pos + x - 1);

  if (tok && tok->tt == t_sync) {
	return parse_peek_x(p, x + 1);
  } else {
	return tok;
  }
}

/// doesn't return null
token* parse_peek(parser* p) {
  return parse_peek_x(p, 1);
}

/// doesn't return null
void parse_next(parser* p) {
  p->current = *parse_peek(p);
  if (p->current.tt != t_eof)
	p->pos++;
}

int peek_sync(parser* p) {
  return ((token*)vector_get(&p->fe->tokens, p->pos))->tt == t_sync;
}

int parse_sync(parser* p) {
  int s = 0;

  while (peek_sync(p)) { //skip synchronization characters
	s = 1;
	p->pos++;
  }

  return s;
}

/// returns 0 if not matched, otherwise sets p.x
int parse_next_eq(parser* p, token_type tt) {
  token* t = parse_peek(p);
  if (t->tt == tt) {
	p->pos++;
	p->current = *t;

	return 1;
  } else {
	return 0;
  }
}

int separator(parser* p) {
  token* t = parse_peek(p);
  return t->tt == t_eof || parse_sync(p);
}

void synchronize(parser* p) {
  while (!separator(p))
	parse_next(p);
}

int try_parse_name(parser* p) {
  token* x = parse_peek(p);
  if (is_name(x)) {
	parse_next(p);
	return 1;
  } else {
	return 0;
  }
}

int try_parse_unqualified(parser* p) {
  token* x = parse_peek(p);
  if (is_name(x) && !x->val.name->qualifier) {
	parse_next(p);
	return 1;
  } else {
	return 0;
  }
}

id* id_access(parser* p, name* x) {
  return map_find(&p->mod->ids, &x->x); //TODO: QUALIFIED
}

unsigned long* parse_unqualified_access(parser* p, char* x) {
  vector_iterator for_iter = vector_iterate(&p->reducers);
  while (vector_next(&for_iter)) {
	reducer* red = for_iter.x;

	if (strcmp(red->name, x) == 0) {
	  return &red->x;
	}
  }

  unsigned long* idx = map_find(p->substitute_idx, &x);
  return idx;
}

expr* parse_left_expr(parser* p, int bind) {
  expr* exp;

  if (parse_next_eq(p, t_for)) {
	// expr --i----\-------O
	// base         \---O
	// inner (step)  \--O

	//create branches
	exp = expr_new_p(p, NULL);
	exp->kind = exp_for;

	exp->_for.i = parse_expr(p, bind, 1);
	if (!exp->_for.i) {
	  throw_here(p, "expected quantifier of for expression");
	  free(exp);
	  return NULL;
	}

	reducer red;
	if (!try_parse_unqualified(p)) {
	  exp->_for.named = 0;
	} else {
	  exp->_for.named = 1;
	  red.name = p->current.val.name->x;
	}

	if (!exp->_for.named || parse_next_eq(p, t_eq)) {
	  exp->_for.base = parse_expr(p, bind, 1);
	  if (!exp->_for.base) {
		throw_here(p, "expected base expression");
		free(exp);
		expr_free(exp->_for.i);
		return NULL;
	  }
	} else {
	  unsigned long* x = parse_unqualified_access(p, red.name);

	  if (!x) {
		throw_here(p, "implicit reference in base of for expression is not defined");
		free(exp);
		expr_free(exp->_for.i);
		return NULL;
	  }

	  exp->_for.base = expr_new_p(p, NULL);

	  exp->_for.base->s = p->current.s;

	  exp->_for.base->kind = exp_add;
	  exp->_for.base->ty = exp_bind;
	  exp->_for.base->val.bind = *x;
	}

	if (exp->_for.named) {
	  red.x = p->substitute_idx->length;
	  vector_pushcpy(&p->reducers, &red); //we use an extra vector to make sure reducers are accessed in order

	  exp->_for.x = red.x;
	  map_insertcpy(p->substitute_idx, &red.name, &red.x);
	}

	exp->first = parse_expr(p, bind, 1);

	if (!exp->first) {
	  free(exp);
	  expr_free(exp->_for.i);
	  expr_free(exp->_for.base);
	  return NULL;
	}

	if (exp->_for.named)
	  vector_pop(&p->reducers);
	map_remove(p->substitute_idx, &red.name);
  } else if (parse_next_eq(p, t_num)) {
	exp = expr_new_p(p, NULL);

	exp->kind = exp_add;
	exp->ty = exp_num;
	exp->val.by = p->current.val.num; //TODO: STRINGS
  } else if (try_parse_name(p)) {
	id* i = id_access(p, p->current.val.name);
	if (i) {
	  if (i->val.substitutes.length > 0) {
		throw_here(p, "cannot substitute name, requires substitutes");
		throw(&i->s, "defined here");
	  }

	  return i->val.val;
	}

	exp = expr_new_p(p, NULL);
	exp->kind = exp_add;
	exp->ty = exp_bind;

	if (bind && !p->current.val.name->qualifier) {
	  exp->val.bind = p->substitute_idx->length; //set value to index
	  map_insertcpy(p->substitute_idx, &p->current.val.name->x, &exp->val.bind);
	} else {
	  unsigned long* a = parse_unqualified_access(p, p->current.val.name->x);
	  if (p->current.val.name->qualifier || !a) {
		throw_here(p, "name does not reference an identifier, reducer, or substitute");
		free(exp);
		return NULL;
	  }

	  exp->kind = exp_add;
	  exp->val.bind = *a;
	}

  } else {
	return NULL; //not an expression, while zero is failure
  }

  exp->s.end = p->current.s.end;
  return exp;
}

expr* parse_expr(parser* p, int do_bind, unsigned op_prec) {
  expr* exp;

  if (parse_next_eq(p, t_sub)) {
	exp = expr_new_p(p, NULL);
	exp->kind = exp_invert;
	exp->first = parse_expr(p, do_bind, op_prec);

	return exp;
  }

  if (parse_next_eq(p, t_add)) {
	exp = expr_new_p(p, NULL);
	exp->kind = exp_add;
	exp->ty = exp_num;
	exp->val.by = &ONE;

	exp->first = parse_expr(p, do_bind, op_prec);
	return exp;
  }

  if (parse_next_eq(p, t_lparen)) {
	span lparen = p->current.s;
	//parentheses around expression, set base precedence to zero
	exp = parse_expr(p, do_bind, 0);
	if (!parse_next_eq(p, t_rparen)) {
	  throw_here(p, "expected ) to end parenthetical");
	  note(&lparen, "left paren here");
	}
  } else {
	exp = parse_left_expr(p, do_bind);
	if (!exp)
	  return NULL;
  }

  //parse ops
  while (1) {
	if (parse_peek(p)->tt == t_rparen) {
	  return exp;
	}

	if (peek_sync(p)) { //probably another identifier's substitutes
	  break;
	}

	token* tok = parse_peek(p);
	if (!is_name(tok)) { //no op
	  break;
	}

	id* applier = id_access(p, tok->val.name); //TODO: qualified appliers

	if (applier) {
	  unsigned int prec = applier->precedence;
	  //dont parse any more
	  if (prec < op_prec)
		break;
	  else
		parse_next(p); //otherwise increment parser

	  expr* new_ex = expr_new_p(p, NULL);
	  new_ex->s.start = exp->s.start;
	  new_ex->kind = exp_call;

	  unsigned subs = applier->val.substitutes.length;

	  new_ex->call.sub.condition = vector_new(sizeof(sub_cond));
	  new_ex->call.sub.val = vector_new(sizeof(expr));

	  if (!bind(exp, *(expr**)vector_get(&applier->val.substitutes, 0), &new_ex->call.sub,
				descend_i(NULL, move_call_i, 0)))
		throw_here(p, "cannot bind substitute");

	  for (unsigned i = 1; i < subs; i++) {
		expr* sub = parse_expr(p, do_bind, prec);
		if (!sub)
		  break;

		if (!bind(sub, *(expr**)vector_get(&applier->val.substitutes, i), &new_ex->call.sub,
				  descend_i(NULL, move_call_i, i)))
		  throw_here(p, "cannot bind substitute");
	  }

	  if (new_ex->call.sub.val.length != applier->val.substitutes.length) {
		throw_here(p,
				   isprintf("expected %lu substitutes, got %lu",
							applier->val.substitutes.length,
							new_ex->call.sub.val.length));
		note(&applier->substitutes, "defined here");
		break;
	  }

	  if (CALL_COST > cost(applier->val.val) && new_ex->call.sub.condition.length == 0) {
		expr* callee = exp_copy(applier->val.val);
		substitute(callee, &new_ex->call.sub);
		exp_rename(callee, p->substitute_idx->length - 1);

		exp = callee;
	  } else {
		new_ex->call.to = &applier->val;
		exp = new_ex;
	  }
	} else if (0 < op_prec) {
	  // error depending on binding strength
	  // this allows things like for loop quantities to return after parsing but identifiers to require parsing
	  return exp;
	} else {
	  throw_here(p, "undefined identifier");
	  return NULL;
	}
  }

  return exp;
}

int parse_id(parser* p) {
  id xid;
  xid.s.start = parse_peek(p)->s.start;

  //reset reducers
  p->reducers = vector_new(sizeof(reducer));

  xid.val.substitutes = vector_new(sizeof(expr*));
  xid.val.substitute_idx = map_new();

  //maps binds name to index for unbiased comparison of ids
  map_configure_string_key(&xid.val.substitute_idx, sizeof(unsigned long));

  p->substitute_idx = &xid.val.substitute_idx;

  //start parsing substitutes
  xid.substitutes.start = parse_peek(p)->s.start;

  token* maybe_eq = parse_peek_x(p, 2);
  if (maybe_eq && maybe_eq->tt != t_eq) {
	expr* exp = parse_expr(p, 1, 1);
	if (exp) {
	  reduce(&exp);
	  vector_pushcpy(&xid.val.substitutes, &exp);
	} else {
	  return throw_here(p, "expected substitute");
	}
  }

  if (try_parse_unqualified(p)) {
	xid.name = p->current.val.name->x;
  } else {
	return throw_here(p, "expected name for identifier");
  }

  while (!parse_next_eq(p, t_eq)) {
	expr* exp = parse_expr(p, 1, 1);
	if (exp) {
	  reduce(&exp);
	  vector_pushcpy(&xid.val.substitutes, &exp);
	} else {
	  return throw_here(p, "expected = or substitute");
	}
  }

  xid.substitutes.end = p->current.s.start;
  span eq = p->current.s;

  xid.val.val = parse_expr(p, 0, 0);
  while (!peek_sync(p) && parse_next_eq(p, t_eq)) { //parse primary alternative
	xid.val.val = parse_expr(p, 0, 0);
  }

  if (!xid.val.val) {
	return throw(&eq, "expected value");
  }

  reduce(&xid.val.val);

  xid.s.end = p->current.s.end;
  map_insertcpy(&p->mod->ids, &xid.name, &xid);

  return 1;
}

void module_init(module* b) {
  b->ids = map_new();
  map_configure_string_key(&b->ids, sizeof(id));
}

int parse_mod(parser* p, module* b) {
  module_init(b);
  module* old_b = p->mod;
  p->mod = b;

  while (!parse_next_eq(p, t_eof)) {
	parse_id(p);

	if (!separator(p)) {
	  synchronize(p);
	  throw_here(p, "expected end of identifier (newline without indentation)");
	}
  }

  p->mod = old_b;
  return 1;
}

void parse(frontend* fe) {
  parser p = {fe, .pos=0};

  module_init(&p.fe->global);
  p.mod = &p.fe->global;

  parse_mod(&p, &p.fe->global);
}

void print_module(module* b) {
  map_iterator ids = map_iterate(&b->ids);
  while (map_next(&ids)) {
	id* xid = ids.x;
	printf("%s ", xid->name);

	vector_iterator subs = vector_iterate(&xid->val.substitutes);
	while (vector_next(&subs)) {
	  print_expr(*(expr**)subs.x);
	  printf(" ");
	}

	printf("= ");

	print_expr(xid->val.val);

	printf("\n");
  }
}