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
	return ((token*) vector_get(&p->fe->tokens, p->pos))->tt == t_sync;
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
	for_iter.rev = 1; //backwards iterator, reducers pushed on top

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

			exp->_for.base->kind = exp_bind;
			exp->_for.base->bind = *x;
		}

		if (exp->_for.named) {
			red.x = p->substitute_idx->length;
			vector_pushcpy(&p->reducers,
										 &red); //we use an extra vector to make sure reducers are accessed in order

			exp->_for.x = red.x;
			map_insertcpy(p->substitute_idx, &red.name, &red.x);
		}

		exp->_for.step = parse_expr(p, bind, 1);

		if (!exp->_for.step) {
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

		exp->kind = exp_num;
		exp->by = p->current.val.num; //TODO: STRINGS
	} else if (try_parse_name(p)) {
		id* i = id_access(p, p->current.val.name);
		if (i) {
			vector_iterator iter = vector_iterate(&i->val);
			while (vector_next(&iter)) {
				value* sub = iter.x;
				if (sub->substitutes.length == 0) {
					//inline expression or set to call without substitutes depending on whether identifier has expression
					if (sub->exp) return exp_copy(sub->exp);
					else {
						exp = expr_new_p(p, NULL);
						call_new(exp, i);

						return exp;
					}
				}
			}

			throw_here(p,
								 "cannot access identifier because it requires substitutes, maybe try applying");
			note(&i->s, "defined here");
			return NULL;
		}

		exp = expr_new_p(p, NULL);
		exp->kind = exp_bind;

		if (bind && !p->current.val.name->qualifier) {
			exp->bind = p->substitute_idx->length; //set value to index
			map_insertcpy(p->substitute_idx, &p->current.val.name->x, &exp->bind);
		} else {
			unsigned long* a = parse_unqualified_access(p, p->current.val.name->x);
			if (p->current.val.name->qualifier || !a) {
				throw_here(p, "name does not reference an identifier, reducer, or substitute");
				free(exp);
				return NULL;
			}

			exp->bind = *a;
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
		exp->inner = parse_expr(p, do_bind, op_prec);

		return exp;
	}

	if (parse_next_eq(p, t_add)) {
		exp = expr_new_p(p, NULL);
		exp->kind = exp_add;

		exp->binary.right = expr_new();
		exp->binary.right->kind = exp_num;
		exp->binary.right->by = &ONE;

		exp->binary.left = parse_expr(p, do_bind, op_prec);
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

		token* applier_name = parse_peek(p);
		if (!is_name(applier_name)) { //no op
			break;
		}

		id* applier = id_access(p, applier_name->val.name); //TODO: qualified appliers

		if (applier) {
			unsigned int prec = applier->precedence;
			//dont parse any more
			if (prec < op_prec)
				break;
			else
				parse_next(p); //otherwise increment parser

			expr* old_exp = exp;
			exp = expr_new_p(p, NULL);
			exp->s.start = exp->s.start;
			exp->kind = exp_call;

			call_new(exp, applier);

			int only = applier->val.length == 1; //provide more debugging info if there is only one dispatch
			int did_bind = 0; //whether inline or call is successful

			vector_iterator iter = vector_iterate(&applier->val);

			//try to bind to one potential at a time
			while (vector_next(&iter)) {
				value* val = iter.x;

				substitution* sub = vector_push(&exp->call.sub);
				sub->val = vector_new(sizeof(expr*));
				sub->static_ = 1;
				sub->to = val;

				vector_pushcpy(&sub->val, &old_exp);

				//bind original expression
				if (!static_condition(sub, 0, old_exp)) {
					vector_free(&sub->val);
					vector_pop(&exp->call.sub);
					if (only) throw(&old_exp->s, "cannot bind substitute");
					continue;
				}

				//bind right hand side substitutes
				for (unsigned i = 1; i < val->substitutes.length; i++) {
					expr* new_sub = parse_expr(p, do_bind, prec);
					if (!new_sub)
						break;

					vector_pushcpy(&sub->val, &new_sub);

					if (!static_condition(sub, i, new_sub)) {
						vector_free(&sub->val);
						vector_pop(&exp->call.sub);
						if (only) throw(&new_sub->s, "cannot bind substitute");
						continue;
					}
				}

				if (sub->val.length != val->substitutes.length) {
					if (only) {
						throw_here(p,
											 isprintf("expected %lu substitutes, got %lu",
																val->substitutes.length,
																sub->val.length));
						note(&val->s, "defined here");
					}

					vector_free(&sub->val);
					vector_pop(&exp->call.sub);
					continue;
				}

				did_bind = 1;

				if (val->exp && CALL_COST > cost(val->exp) && sub->static_) {

					expr* callee = exp_copy(val->exp);
					exp_rename(callee, val->substitute_idx.length, p->substitute_idx->length - 1);

					substitute(callee, sub);

					exp = callee;
				}

				if (sub->static_) {
					break; //wont get any better than this
				}
			}

			if (!did_bind) {
				throw(&applier->s, "could not bind to callee");
				note(&applier->s, "defined here");
				return NULL;
			}

		} else if (0 < op_prec) {
			// error depending on binding strength
			// this allows things like for loop quantities to return after parsing but identifiers to require parsing
			return exp;
		} else {
			throw(&applier_name->s, "undefined identifier");
			return NULL;
		}
	}

	return exp;
}

int parse_id(parser* p) {
	value val;
	val.s.start = parse_peek(p)->s.start;

	//reset reducers
	p->reducers = vector_new(sizeof(reducer));

	val.groups = vector_new(sizeof(sub_group));
	val.substitutes = vector_new(sizeof(expr*));
	val.substitute_idx = map_new();

	//maps binds name to index for unbiased comparison of ids
	map_configure_string_key(&val.substitute_idx, sizeof(unsigned long));

	p->substitute_idx = &val.substitute_idx;

	token* maybe_eq = parse_peek_x(p, 2);
	if (maybe_eq && maybe_eq->tt != t_eq) {
		expr* exp = parse_expr(p, 1, 1);
		if (exp) {
			reduce(&exp);
			vector_pushcpy(&val.substitutes, &exp);

			gen_condition(&val, exp, 0);
			gen_substitutes(&val, exp, 0);
		} else {
			return throw_here(p, "expected substitute");
		}
	}


	if (!try_parse_unqualified(p))
		return throw_here(p, "expected name for identifier");

	char* name = p->current.val.name->x;
	id* xid = map_find(&p->mod->ids, &name);
	if (!xid) {
		id new_id = {
				.s=p->current.s,
				.val=vector_new(sizeof(value)),
				.name=name,
				.precedence=p->mod->ids.length
		};

		xid = map_insertcpy(&p->mod->ids, &name, &new_id).val;
	}

	int eq;
	unsigned long i = 1;

	while (!(eq = parse_next_eq(p, t_eq)) && !parse_sync(p)) {
		expr* exp = parse_expr(p, 1, 1);
		if (exp) {
			reduce(&exp);
			vector_pushcpy(&val.substitutes, &exp);
			//generation phase
			gen_condition(&val, exp, i++);
			gen_substitutes(&val, exp, i++);
		} else {
			return throw_here(p, "expected = or substitute");
		}
	}

	if (eq) {
		span eq_s = p->current.s;

		val.exp = parse_expr(p, 0, 0);
		while (!peek_sync(p) && parse_next_eq(p, t_eq)) { //parse primary alternative
			val.exp = parse_expr(p, 0, 0);
		}

		if (!val.exp) {
			throw_here(p, "expected value following equals sign");
			note(&eq_s, "equals sign here");

			return 0;
		}

		reduce(&val.exp);
	} else {
		val.exp = NULL;
	}

	val.s.end = p->current.s.end;

	vector_pushcpy(&xid->val, &val);

	return 1;
}

int parse_mod(parser* p, module* b) {
	while (!parse_next_eq(p, t_eof)) {
		parse_id(p);

		if (!separator(p)) {
			synchronize(p);
			throw_here(p, "expected end of identifier (newline without indentation)");
		}
	}

	return 1;
}

void parse(frontend* fe) {
	parser p = {fe, .pos=0};
	p.mod = &p.fe->global;

	parse_mod(&p, &p.fe->global);
}

void print_module(module* b) {
	map_iterator ids = map_iterate(&b->ids);
	while (map_next(&ids)) {
		id* xid = ids.x;
		printf("%s ", xid->name);

		vector_iterator vals = vector_iterate(&xid->val);
		while (vector_next(&vals)) {
			value* val = vals.x;
			vector_iterator subs = vector_iterate(&val->substitutes);
			while (vector_next(&subs)) {
				print_expr(*(expr**) subs.x);
				printf(" ");
			}

			if (val->exp) {
				printf("= ");
				print_expr(val->exp);
			}
		}

		printf("\n");
	}
}