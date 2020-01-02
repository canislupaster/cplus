#import "expr.h"

expr* expr_new() {
	expr* x = heap(sizeof(expr));

	x->cost = 0;

	x->s.start = NULL;
	x->s.end = NULL;
	return x;
}

expr* expr_new_p(parser* p, expr* first) {
	expr* x = expr_new();
	x->s.start = first ? first->s.start : p->current.s.start;

	return x;
}

//CLASSIFIERS

int is_value(expr* exp) {
	return exp->kind == exp_num || exp->kind == exp_bind;
}

int binary(expr* exp) {
	return exp->kind == exp_add || exp->kind == exp_mul || exp->kind == exp_div || exp->kind == exp_pow;
}

int unary(expr* exp) {
	return exp->kind == exp_invert;
}

int def(expr* exp) {
	return exp->kind == exp_for || exp->kind == exp_def || exp->kind == exp_cond;
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
		case move_inner: return root->inner;

		case move_left: return root->binary.left;
		case move_right: return root->binary.right;

		case move_for_step: return root->_for.step;
		case move_for_base: return root->_for.base;
		case move_for_i: return root->_for.i;

		case move_call_i: return *(expr**) vector_get(&root->call.sub.val, where->i);
	}
}

expr_iterator exp_iter(expr* exp) {
	expr_iterator iter = {.cursor=NULL, .root=exp, .done=0, .sub_done=vector_new(sizeof(unsigned long))};
	return iter;
}

void exp_ascend(expr_iterator* iter) {
	//if we've already fully A S C E N D E D the whole thing is done
	if (!iter->cursor) {
		iter->done = 1;
		return;
	}

	iter->cursor = iter->cursor->from;
	expr* up = goto_idx(iter->root, iter->cursor);

	if (unary(up))
		return exp_ascend(iter);

	unsigned long* last = vector_get(&iter->sub_done, iter->sub_done.length - 1);

	if (binary(up)) {
		if (*last == 1) {
			vector_pop(&iter->sub_done);
			return exp_ascend(iter);
		} else {
			*last = 1;
			iter->cursor = descend(iter->cursor, move_right);
		}
	}

	switch (up->kind) {
		case exp_cond:
		case exp_def:
		case exp_for: {
			if (*last == 2) {
				vector_pop(&iter->sub_done);
				return exp_ascend(iter);
			} else if (*last == 1) {
				*last = 2;
				iter->cursor = descend(iter->cursor, move_for_i);
			} else if (*last == 0) {
				*last = 1;
				iter->cursor = descend(iter->cursor, move_for_base);
			}

			break;
		}
		case exp_call: {
			if (*last == up->call.sub.val.length) {
				vector_pop(&iter->sub_done);
				return exp_ascend(iter);
			} else {
				*last = *last + 1; //assign to reference
				iter->cursor = descend_i(iter->cursor, move_call_i, *last);
			}
		}

		default:;
	}
}

int exp_next(expr_iterator* iter) {
	if (iter->done) return 0;

	expr* this = goto_idx(iter->root, iter->cursor);
	iter->x = this;

	//leaf, ascend
	if (is_value(this)) {
		exp_ascend(iter);
	} else if (unary(this)) {
		iter->cursor = descend(iter->cursor, move_inner);
	} else {
		unsigned long init = 0;
		vector_pushcpy(&iter->sub_done, &init);

		if (binary(this)) {
			iter->cursor = descend(iter->cursor, move_left);
		}

		switch (this->kind) {
			case exp_cond:
			case exp_def:
			case exp_for: {
				iter->cursor = descend(iter->cursor, move_for_step);
				break;
			}
			case exp_call: {
				if (this->call.sub.val.length == 0) {
					vector_pop(&iter->sub_done); //undo insertion
					exp_ascend(iter);
				} else
					iter->cursor = descend_i(iter->cursor, move_call_i, 0);
			}

			default:;
		}
	}


	return 1;
}

expr* exp_copy(expr* exp) {
	expr* new_exp = heapcpy(sizeof(expr), exp);
	*new_exp = *exp;

	if (binary(exp)) {
		new_exp->binary.left = exp_copy(exp->binary.left);
		new_exp->binary.right = exp_copy(exp->binary.right);
	}

	switch (new_exp->kind) {
		case exp_cond:
		case exp_def:
		case exp_for: {
			new_exp->_for.i = exp_copy(new_exp->_for.i);
			new_exp->_for.base = exp_copy(new_exp->_for.base);
			new_exp->_for.step = exp_copy(new_exp->_for.step);

			break;
		}
		case exp_call: {
			vector_cpy(&exp->call.sub.condition, &new_exp->call.sub.condition);
			vector_cpy(&exp->call.sub.val, &new_exp->call.sub.val);

			vector_iterator iter = vector_iterate(&new_exp->call.sub.val);
			while (vector_next(&iter)) {
				*(expr**) iter.x = exp_copy(*(expr**) iter.x);
			}
		}

		default:;
	}

	return new_exp;
}

/// uniquely renames identifiers in loops n stuff >= the specified threshold
/// this should be used to separate reducer spaces, with the threshold above the length of substitutes
void exp_rename(expr* exp, unsigned threshold, unsigned offset) {
	expr_iterator iter = exp_iter(exp);
	while (exp_next(&iter)) {
		if (iter.x->kind == exp_bind && iter.x->bind >= threshold) {
			iter.x->bind += offset;
		} else if (def(iter.x) && iter.x->_for.x >= threshold) {
			iter.x->_for.x += offset;
		}
	}
}

int bind(expr* from, expr* to, substitution* sub) {
	if (binary(from) && binary(to)) {
		if (from->kind != to->kind)
			return 0;

		if (!bind(from->binary.left, to->binary.left, sub))
			return 0;
		if (!bind(from->binary.right, to->binary.right, sub))
			return 0;
	} else if (to->kind == exp_bind) {
		vector_pushcpy(&sub->val, &from);
	} else if (from->kind == exp_bind) { //make sure from == to at runtime
		vector_pushcpy(&sub->condition,
									 &(sub_cond) {.what=to, .x = from->bind});
	} else {
		if (to->kind != from->kind)
			return 0;

		switch (from->kind) {
			case exp_call: {
				if (from->call.to != to->call.to)
					return 0;

				vector_iterator iter = vector_iterate(&from->call.sub.val);
				while (vector_next(&iter)) {
					expr* exp_2 = *(expr**) vector_get(&to->call.sub.val, iter.i - 1);
					if (!exp_2)
						return 0;

					if (!bind(iter.x, exp_2, sub))
						return 0;
				}
			}

			case exp_cond:
			case exp_def:
			case exp_for: {
				//x should always be the same, match on base and i
				if (!bind(from->_for.step, to->_for.step, sub)
						|| !bind(from->_for.base, to->_for.base, sub)
						|| !bind(from->_for.i, to->_for.i, sub))
					return 0;

				break;
			}

			case exp_invert: break;
			default:;
		}
	}


	return 1;
}

//substitutes in reverse in comparison to bind function
int substitute(expr* exp, substitution* sub) {
	if (sub->condition.length > 0)
		return 0;

	expr_iterator iter = exp_iter(exp);
	while (exp_next(&iter)) {
		if (iter.x->kind == exp_bind && iter.x->bind < sub->val.length) {
			*iter.x = **(expr**) vector_get(&sub->val, iter.x->bind);
		}
	}

	return 1;
}

int binding_exists(expr* exp, unsigned long x) {
	expr_iterator iter = exp_iter(exp);
	while (exp_next(&iter)) {
		if (iter.x->kind == exp_bind && iter.x->bind == x) {
			return 1;
		}
	}

	return 0;
}

binary_iterator binary_iter(expr* exp) {
	return (binary_iterator) {.right=0, .exp=exp};
}

int binary_next(binary_iterator* iter) {
	if (iter->right == 1) {
		iter->x = iter->exp->binary.right;
		iter->other = iter->exp->binary.left;
	} else if (!iter->right) {
		iter->x = iter->exp->binary.left;
		iter->other = iter->exp->binary.right;
	} else {
		return 0;
	}

	iter->right++;
	return 1;
}

//extracts operand on other side (than x1) of operator
expr* extract_operand(expr* exp, unsigned long x) {
	if (!binary(exp)) return NULL;

	binary_iterator iter = binary_iter(exp);
	while (binary_next(&iter)) {
		if (iter.x->kind == exp_bind && iter.x->bind == x
				&& !binding_exists(iter.other, x)) {

			return iter.other;
		}
	}

	return NULL;
}

// in place numerical operand remover
int remove_num(expr** eref, num* num) {
	expr* exp = *eref;

	if (!binary(exp)) return 0;

	binary_iterator iter = binary_iter(exp);
	while (binary_next(&iter)) {
		if (iter.x->kind == exp_num && num_eq(*iter.x->by, *num)) {
			*eref = iter.other;
			return 1;
		}
	}

	return 0;
}

void print_expr(expr* exp) {
	if (exp->kind == exp_invert) {
		printf("-");
		print_expr(exp->inner);
	} else if (binary(exp)) {
		printf("(");
		print_expr(exp->binary.left);
		printf(")");

		switch (exp->kind) {
			case exp_add: printf("+");
				break;
			case exp_div: printf("/");
				break;
			case exp_mul: printf("*");
				break;
			case exp_pow: printf("^");
				break;
			default:;
		}

		printf("(");
		print_expr(exp->binary.right);
		printf(")");
	} else {
		switch (exp->kind) {
			case exp_for: {
				print_expr(exp->_for.step);
				printf(" for (x=@%lu) from ", exp->_for.x);
				print_expr(exp->_for.base);
				printf(", ");
				print_expr(exp->_for.i);
				printf(" times");

				break;
			}

			case exp_cond: {
				print_expr(exp->_for.step);
				printf(" if (x=@%lu) ", exp->_for.x);
				print_expr(exp->_for.i);
				printf(" else ");
				print_expr(exp->_for.base);

				break;
			}

			case exp_def: {
				print_expr(exp->_for.step);
				printf(" where @%lu = ", exp->_for.x);
				print_expr(exp->_for.base);

				break;
			}

			case exp_call: {
				printf("call ");

				vector_iterator iter = vector_iterate(&exp->call.sub.val);
				while (vector_next(&iter)) {
					print_expr(*(expr**) iter.x);
					printf(" ");
				}

				break;
			}

			case exp_num: print_num((num*) exp->by);
				break;
			case exp_bind: printf("@%lu", exp->bind);
				break;

			default:;
		}
	}
}

void expr_free(expr* exp) {
	if (unary(exp)) {
		expr_free(exp->inner);
	}
	if (binary(exp)) {
		expr_free(exp->binary.left);
		expr_free(exp->binary.right);
	} else {
		switch (exp->kind) {
			case exp_for: {
				expr_free(exp->_for.step);
				expr_free(exp->_for.base);
				expr_free(exp->_for.i);

				break;
			}
			case exp_call: {
				vector_iterator iter = vector_iterate(&exp->call.sub.val);
				while (vector_next(&iter)) {
					expr_free(*(expr**) iter.x);
				}

				vector_free(&exp->call.sub.val);

				break;
			}
			default:;
		}
	}


	free(exp);
}