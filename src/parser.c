#include "parser.h"

int throw_here(parser* p, const char* x) {
  return throw(&p->current.s, x);
}

token* parse_peek_x(parser* p, int x) {
  token* tok = vector_get(&p->fe->tokens, p->pos+x-1);

  if (tok && tok->tt == t_sync) {
	return parse_peek_x(p, x+1);
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
  if (p->current.tt != t_eof) p->pos++;
}

int peek_sync(parser *p) {
  return ((token*)vector_get(&p->fe->tokens, p->pos))->tt == t_sync;
}

int parse_sync(parser *p) {
  int s = 0;

  while (peek_sync(p)) { //skip synchronization characters
    s=1; p->pos++;
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

const expr EXPR_ONE = {.kind=exp_add, .val = {.by = &ONE}};
const expr EXPR_ZERO = {.kind=exp_add, .val = {.by = &ZERO}};

int binary(expr* exp) {
  return exp->kind != exp_for && exp->kind != exp_invert && exp->kind != exp_call;
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

int bind(expr* from, expr* to, substitution* sub, exp_idx* cursor) {
  if (from->kind == exp_call) {
	//do thing
	return 1;
  }

  if (to->kind != from->kind) return 0;

  if (from->first && to->first) {
    bind(from->first, to->first, sub, descend(cursor, move_left));
  } else if (from->first && !to->first) {
    return 0;
  }

  switch (from->kind) {
  case exp_cond:
  case exp_def:
  case exp_for: {
    //x should always be the same, match on base and i
	if (!bind(from->_for.base, to->_for.base, sub, descend(cursor, move_for_base))) return 0;
	if (!bind(from->_for.i, to->_for.i, sub, descend(cursor, move_for_i))) return 0;

	break;
  }

  case exp_invert: break;

  default: {
	if (to->ty == exp_bind) { //bind from to to
	  vector_pushcpy(&sub->val, &from);
	} else if (from->ty == exp_bind) { //make sure from == to at runtime
	  sub_cond cond = {.where = cursor};
	  vector_pushcpy(&sub->condition, &cond);
	} else if (from->ty == to->ty) {
	  switch (from->ty) {
	  case exp_num: return num_eq(*from->val.by, *to->val.by);
	  case exp_inner: return bind(from->val.inner, to->val.inner, sub, descend(cursor, move_right));
	  default:;
	  }
	} else {
	  return 0;
	}
  }
  }

  return 1;
}

expr* exp_copy(expr* exp) {
  expr* new_exp = heapcpy(sizeof(expr), exp);
  *new_exp = *exp;

  if (exp->first) new_exp->first = exp_copy(exp->first);

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

  if (exp->first) substitute(exp->first, sub);

  return 1;
}

int binding_exists(expr* e, unsigned long x) {
  if (binary(e)) {
    if (e->ty == exp_bind && e->val.bind == x) return 1;
    if (e->ty == exp_inner && binding_exists(e->val.inner, x)) return 1;
  }

  if (e->first) return binding_exists(e->first, x);
  else return 0;
}

//simple recursive binary function to remove a binding once from a (binary) expression
//returns whether it has been removed completely
int replace_binding(expr* e, unsigned long x1, expr* e2) {
  //only descend one layer max
  if (e->first && !e->first->first) {
	if (replace_binding(e->first, x1, e2))
	  return !binding_exists(e->first, x1);
  }

  if (binary(e) && e->ty == exp_bind && e->val.bind == x1) {
	e->ty = exp_inner;
	e->val.inner = e2;

	return e->first ? !binding_exists(e->first, x1) : 1;
  }

  return 0;
}

int remove_num(expr** eref, num* num) {
  expr* e = *eref;
  //only descend one layer max
  if (e->first && !e->first->first) {
	if (e->first->ty == exp_num && num_eq(*e->first->val.by, *num)) {
	  expr_free(e->first);
	  e->first = NULL;

	  return 1;
	}
  }

  if (binary(e) && e->ty == exp_num && num_eq(*e->val.by, *num)) {
	*eref = e->first;
	expr_head_free(e);
  }

  return 0;
}

expr* goto_idx(expr* root, exp_idx* where) {
  if (!where) return root;

  root = goto_idx(root, where->from);

  switch (where->kind) {
  case move_left:
	return root->first;
  case move_right: {
    if (root->ty == exp_inner) return root->val.inner;
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

  case exp_for: e->cost = acc * cost(e->_for.i) + cost(e->_for.base); break;
  case exp_cond: e->cost = acc + cost(e->_for.i) + cost(e->_for.base); break;
  case exp_def: e->cost = acc + cost(e->_for.base); break;

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
  if (!e->first && binary(e) && e->ty == exp_inner) {
	e = e->val.inner;
  }

  if (e->first)
	opt_reduce(&e->first, opt);

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
      e->val.by = num_new(invert(*amount->val.by));
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
  if (binary(e) && e->ty == exp_inner && binary(e->val.inner) && !e->val.inner->first) {
	expr* inner = e->val.inner;
	e->ty = inner->ty;
	e->val.by = inner->val.by;

	expr_free(inner);
  }

  if (e->first && !e->first->first && e->first->ty == exp_num && (!binary(e) || e->ty==exp_num)) {
	switch (e->kind) {
	case exp_add: set_num(e, add(*e->first->val.by, *e->val.by)); break;
	case exp_mul: set_num(e, mul(*e->first->val.by, *e->val.by)); break;
	case exp_div: set_num(e, num_div(*e->first->val.by, *e->val.by)); break;
	case exp_pow: set_num(e, num_pow(*e->first->val.by, *e->val.by)); break;
	case exp_invert: set_num(e, invert(*e->first->val.by)); break;

	default:;
	}
  }

  if (e->kind == exp_mul || e->kind == exp_div) {
	remove_num(eref, &ONE); e=*eref;
  } else if (e->kind == exp_add && e->first) {
	remove_num(eref, &ZERO); e=*eref;
  }

  //TODO: simplify to def if i is 1
  switch (e->kind) {
  case exp_for: {
	reduce(&e->_for.base);
	reduce(&e->_for.i);

	if (!e->_for.named || !map_find(&opt->usages, &e->_for.x)) {
	  e->kind = exp_cond; //expression can be simplified to a conditional if step (first) does not use x

	  if (!e->_for.i->first && e->_for.i->ty == exp_num && num_gt(*e->_for.i->val.by, ZERO)) {
		e->kind = exp_def;
	  }
	} else if (e->first->kind == exp_add) {
	  //reduce repeated addition to multiplication
	  if (!replace_binding(e->first, e->_for.x, e->_for.i)) break;
	  e->first->kind = exp_mul;

	  expr* first = e->first;
	  expr* base = e->_for.base;
	  free(e);

	  e = expr_new(first);
	  e->kind = exp_add;
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

  if (binary(e) && e->ty == exp_inner) {
	opt_reduce(&e->val.inner, opt);
  } else if (binary(e) && e->ty == exp_bind) {
	map_insert(&opt->usages, &e->val.bind);
  }

  *eref = e;
}

void reduce(expr** exp) {
  optimizer opt = {.usages=map_new()};
  map_configure_ulong_key(&opt.usages, 0);
  opt_reduce(exp, &opt);
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

unsigned long* unqualified_access(parser* p, char* x) {
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
  expr* ex;

  if (parse_next_eq(p, t_for)) {
	// expr --i----\-------O
	// base         \---O
	// inner (step)  \--O

	//create branches
	ex = expr_new_p(p, NULL);
	ex->kind = exp_for;

	ex->_for.i = parse_expr(p, bind, 1);
	if (!ex->_for.i) {
	  throw_here(p, "expected quantifier of for expression");
	  free(ex);
	  return NULL;
	}

	reducer red;
	if (!try_parse_unqualified(p)) {
	  ex->_for.named=0;
	} else {
	  red.name = p->current.val.name->x;
	}

	if (!ex->_for.named || parse_next_eq(p, t_eq)) {
	  ex->_for.base = parse_expr(p, bind, 1);
	  if (!ex->_for.base) {
		throw_here(p, "expected base expression");
		free(ex);
		expr_free(ex->_for.i);
		return NULL;
	  }
	} else {
	  unsigned long* x = unqualified_access(p, red.name);

	  if (!x) {
		throw_here(p, "implicit reference in base of for expression is not defined");
		free(ex);
		expr_free(ex->_for.i);
		return NULL;
	  }

	  ex->_for.base = expr_new_p(p, NULL);

	  ex->_for.base->s = p->current.s;

	  ex->_for.base->kind = exp_add;
	  ex->_for.base->ty = exp_bind;
	  ex->_for.base->val.bind = *x;
	}

	if (ex->_for.named) {
	  red.x = p->substitute_idx->length;
	  vector_pushcpy(&p->reducers, &red); //we use an extra vector to make sure reducers are accessed in order

	  ex->_for.x = red.x;
	  map_insertcpy(p->substitute_idx, &red.name, &red.x);
	}

	ex->first = parse_expr(p, bind, 1);

	if (!ex->first) {
	  free(ex);
	  expr_free(ex->_for.i);
	  expr_free(ex->_for.base);
	  return NULL;
	}

	if (ex->_for.named) vector_pop(&p->reducers);
	map_remove(p->substitute_idx, &red.name);
  } else if (parse_next_eq(p, t_num)) {
	ex = expr_new_p(p, NULL);

	ex->kind = exp_add;
	ex->ty = exp_num;
	ex->val.by = p->current.val.num; //TODO: STRINGS
  } else if (try_parse_name(p)) {
	id* i = id_access(p, p->current.val.name);
	if (i) {
	  if (i->val.substitutes.length > 0) {
		throw_here(p, "cannot substitute name, requires substitutes");
		throw(&i->s, "defined here");
	  }

	  return i->val.val;
	}

	ex = expr_new_p(p, NULL);
	ex->kind = exp_add;
	ex->ty = exp_bind;

	if (bind && !p->current.val.name->qualifier) {
	  ex->val.bind = p->substitute_idx->length; //set value to index
	  map_insertcpy(p->substitute_idx, &p->current.val.name->x, &ex->val.bind);
	} else {
	  unsigned long* a = unqualified_access(p, p->current.val.name->x);
	  if (p->current.val.name->qualifier || !a) {
		throw_here(p, "name does not reference an identifier, reducer, or substitute");
		free(ex);
		return NULL;
	  }

	  ex->kind = exp_add;
	  ex->val.bind = *a;
	}

  } else {
	return NULL; //not an expression, while zero is failure
  }

  ex->s.end = p->current.s.end;
  return ex;
}

expr* parse_expr(parser* p, int do_bind, unsigned op_prec) {
  expr* ex;

  if (parse_next_eq(p, t_sub)) {
	ex = expr_new_p(p, NULL);
	ex->kind = exp_invert;
	ex->first = parse_expr(p, do_bind, op_prec);

	return ex;
  }

  if (parse_next_eq(p, t_add)) {
	ex = expr_new_p(p, NULL);
	ex->kind = exp_add;
	ex->ty = exp_num;
	ex->val.by = &ONE;

	ex->first = parse_expr(p, do_bind, op_prec);
	return ex;
  }

  if (parse_next_eq(p, t_lparen)) {
	span lparen = p->current.s;
	//parentheses around expression, set base precedence to zero
	ex = parse_expr(p, do_bind, 0);
	if (!parse_next_eq(p, t_rparen)) {
	  throw_here(p, "expected ) to end parenthetical");
	  note(&lparen, "left paren here");
	}
  } else {
	ex = parse_left_expr(p, do_bind);
	if (!ex)
	  return NULL;
  }

  //parse ops
  while (1) {
	if (parse_peek(p)->tt == t_rparen) {
	  return ex;
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
	  new_ex->s.start = ex->s.start;
	  new_ex->kind = exp_call;

	  unsigned subs = applier->val.substitutes.length;

	  new_ex->call.sub.condition = vector_new(sizeof(sub_cond));
	  new_ex->call.sub.val = vector_new(sizeof(expr));

	  if (!bind(ex, *(expr**)vector_get(&applier->val.substitutes, 0), &new_ex->call.sub,
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
		exp_rename(callee, p->substitute_idx->length-1);

		ex = callee;
	  } else {
		new_ex->call.to = &applier->val;
		ex = new_ex;
	  }
	} else if (0 < op_prec) {
	  // error depending on binding strength
	  // this allows things like for loop quantities to return after parsing but identifiers to require parsing
	  return ex;
	} else {
	  throw_here(p, "undefined identifier");
	  return NULL;
	}
  }

  return ex;
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
	expr* e = parse_expr(p, 1, 1);
	if (e) {
	  vector_pushcpy(&xid.val.substitutes, &e);
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
	expr* e = parse_expr(p, 1, 1);
	if (e) {
	  vector_pushcpy(&xid.val.substitutes, &e);
	} else {
	  return throw_here(p, "expected = or substitute");
	}
  }

  xid.substitutes.end = p->current.s.start;
  span eq = p->current.s;

  xid.val.val = parse_expr(p, 0, 0);
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

void print_expr(expr* e) {
  printf("(");

  if (e->kind == exp_invert) {
	printf("-");
  }

  if (e->first)
	print_expr(e->first);

  switch (e->kind) {
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
	printf(" for (x=%lu) from ", e->_for.x);
	print_expr(e->_for.base);
	printf(", ");
	print_expr(e->_for.i);
	printf(" times");

	break;
  }

  case exp_cond: {
	printf(" if (x=%lu) ", e->_for.x);
	print_expr(e->_for.i);
	printf(" else ");
	print_expr(e->_for.base);

	break;
  }

  case exp_def: {
    printf("where @%lu = ", e->_for.x);
    print_expr(e->_for.base);

    break;
  }

  case exp_call: {
	printf("call ");

	vector_iterator iter = vector_iterate(&e->call.sub.val);
	while (vector_next(&iter)) {
	  print_expr(*(expr**)iter.x);
	  printf(" ");
	}

	break;
  }
  }

  if (binary(e)) {
	switch (e->ty) {
	case exp_num: print_num((num*)e->val.by);
	  break;
	  //        case : printf("\"%s\"", (char*)e->val.str); break;
	case exp_inner: {
	  printf("(");
	  print_expr((expr*)e->val.inner);
	  printf(")");
	  break;
	}
	case exp_bind: printf("@%lu", e->val.bind);
	  break;
	}
  }

  printf(")");
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