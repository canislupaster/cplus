#import "vector.h"
#import "hashtable.h"
#import "util.h"

#import "numbers.h"
#import "parser.h"
#import "lexer.h"

#import <stdlib.h>

typedef enum {
	move_left, move_right, move_inner,
	move_for_i, move_for_base, move_for_step,
	move_call_i
} move_kind;

/// substituted in reverse
typedef struct {
	struct value* to;
	char static_; //whether it can be inlined / passes all conditions statically
	vector val; //expression for every substitute indexes
} substitution;

typedef enum kind {
	exp_bind, exp_num,
	exp_add, exp_invert, exp_mul, exp_div, exp_pow, //1-2 args
	//a conditional is a for expressed without the base, def is a for if i=1
	exp_cond, exp_def, exp_for, exp_call //2-3 args
} kind;

/// everything has one to three arguments and can be chained
typedef struct expr {
	span s;
	int cost; //memoized cost

	kind kind;

	union {
		num* by;
		unsigned long bind;
		struct expr* inner;

		struct {
			struct expr* base; //if zero
			struct expr* step;

			char named;
			unsigned long x;
			struct expr* i;
		} _for;

		struct {
			struct expr* left;
			struct expr* right;
		} binary;

		struct {
			struct id* to;
			vector sub; // multiple dispatch, iterate until condition checks
		} call;
	};
} expr;

typedef struct exp_idx {
	struct exp_idx* from;
	move_kind kind;
	unsigned long i; //index of value
	unsigned long i2; //index of substitute
} exp_idx;

typedef struct expr_iterator {
	expr* root;
	expr* x;
	exp_idx* cursor;

	vector sub_done; //left, then right, (or step, base, i) then up the cursor, pop done
	char done;
} expr_iterator;

typedef struct {
	expr* exp;
	expr* x;
	expr* other;

	char right;
} binary_iterator;

typedef struct sub_idx {
	unsigned int i;
	exp_idx* idx;
} sub_idx;

typedef struct {
	exp_idx* idx;

	expr* exp; //make sure it is equivalent to an expression at leaf-binding
	kind kind; //otherwise check kind and descend through substitute indexes
} sub_cond;

typedef struct sub_group {
	vector condition;
} sub_group;

/// identifier or expr (empty vec and map)
typedef struct value {
	span s;
	vector groups; //conditions for substitutes in each expression
	vector substitutes; //vector of sub_idx specifying substitutes

	map substitute_idx;

	struct expr* exp;
} value;

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

void call_new(expr* exp, id* i) {
	exp->kind = exp_call;

	exp->call.sub = vector_new(sizeof(substitution));
	exp->call.to = i;
}

//CLASSIFIERS

int is_literal(expr* exp) {
	//check that all potential substitutions do not have any equivalent expressions; as simplified as can be
	if (exp->kind == exp_call) {
		vector_iterator iter = vector_iterate(&exp->call.sub);
		while (vector_next(&iter)) {
			if (((substitution*) iter.x)->to->exp != NULL) return 0;
		}

		return 1;
	}

	return exp->kind == exp_num;
}

int is_value(expr* exp) {
	return is_literal(exp) || exp->kind == exp_bind;
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

exp_idx* exp_idx_copy(exp_idx* from) {
	if (!from) return NULL;

	exp_idx* new_idx = heapcpy(sizeof(exp_idx), from);
	if (from->from) new_idx->from = exp_idx_copy(from->from);

	return new_idx;
}

exp_idx* descend(exp_idx* start, move_kind kind) {
	exp_idx* new = heap(sizeof(exp_idx));
	new->from = start;
	new->kind = kind;

	return new;
}

exp_idx* descend_i(exp_idx* start, move_kind kind, unsigned long i, unsigned long i2) {
	exp_idx* new = descend(start, kind);
	new->i = i;
	new->i2 = i2;

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

		case move_call_i: {
			substitution* sub = vector_get(&root->call.sub, where->i);
			return *(expr**) vector_get(&sub->val, where->i2);
		}
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

			if (*last == up->call.sub.length) {
				vector_pop(&iter->sub_done); //pop both indices
				vector_pop(&iter->sub_done);

				return exp_ascend(iter);
			} else {
				substitution* sub = vector_get(&up->call.sub, *last);

				unsigned long* last2 = vector_get(&iter->sub_done, iter->sub_done.length - 2);
				if (*last2 == sub->val.length) {
					*last = *last + 1; //assign to reference
				} else {
					*last2 = *last2 + 1;
				}

				iter->cursor = descend_i(iter->cursor, move_call_i, *last, *last2);
			}
		}

		default:;
	}
}

int exp_get(expr_iterator* iter) {
	if (iter->done) return 0;

	expr* this = goto_idx(iter->root, iter->cursor);
	iter->x = this;

	return 1;
}

void exp_go(expr_iterator* iter) {
	//leaf, ascend
	if (is_value(iter->x)) {
		exp_ascend(iter);
	} else if (unary(iter->x)) {
		iter->cursor = descend(iter->cursor, move_inner);
	} else {
		unsigned long init = 0;
		vector_pushcpy(&iter->sub_done, &init);

		if (binary(iter->x)) {
			iter->cursor = descend(iter->cursor, move_left);
		}

		switch (iter->x->kind) {
			case exp_cond:
			case exp_def:
			case exp_for: {
				iter->cursor = descend(iter->cursor, move_for_step);
				break;
			}
			case exp_call: {

				if (((substitution*) vector_get(&iter->x->call.sub, 0))->val.length == 0) {
					vector_pop(&iter->sub_done); //undo insertion
					exp_ascend(iter);
				} else {
					vector_pushcpy(&iter->sub_done, &init); //push again
					iter->cursor = descend_i(iter->cursor, move_call_i, 0, 0);
				}
			}

			default:;
		}
	}
}

int exp_next(expr_iterator* iter) {
	if (iter->done) return 0;

	expr* this = goto_idx(iter->root, iter->cursor);
	iter->x = this;

	exp_go(iter);

	return 1;
}

//copies expression by value
expr exp_copy_value(expr* exp) {
	expr new_exp = *exp;

	if (binary(exp)) {
		new_exp.binary.left = exp_copy(exp->binary.left);
		new_exp.binary.right = exp_copy(exp->binary.right);
	}

	switch (new_exp.kind) {
		case exp_cond:
		case exp_def:
		case exp_for: {
			new_exp._for.i = exp_copy(new_exp._for.i);
			new_exp._for.base = exp_copy(new_exp._for.base);
			new_exp._for.step = exp_copy(new_exp._for.step);

			break;
		}
		case exp_call: {
			vector_cpy(&exp->call.sub, &new_exp.call.sub);
			vector_iterator vals = vector_iterate(&new_exp.call.sub);
			while (vector_next(&vals)) {
				substitution* sub = vector_get(&exp->call.sub, vals.i - 1);
				substitution* new_sub = vals.x;

				vector_cpy(&sub->val, &new_sub->val);

				vector_iterator iter = vector_iterate(&new_sub->val);
				while (vector_next(&iter)) {
					expr* old_exp = *(expr**) iter.x;
					*(expr**) iter.x = exp_copy(old_exp);
				}
			}

		}

		default:;
	}

	return new_exp;
}

expr* exp_copy(expr* exp) {
	expr new_exp = exp_copy_value(exp);
	return heapcpy(sizeof(expr), &new_exp);
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

void gen_condition(value* val, expr* bind_exp, unsigned int i) {
	sub_group* group = vector_setcpy(&val->groups, i,
																	 &(sub_group) {.condition=vector_new(sizeof(sub_cond))});

	expr_iterator iter = exp_iter(bind_exp);
	while (!iter.done) {
		exp_get(&iter);

		sub_cond cond = {.idx=exp_idx_copy(iter.cursor)}; //FIXME: MEMORY LEAKING INDEXES EVERYWHERE

		//insert expression to check if literal, otherwise checking the kind is enough
		if (is_literal(iter.x)) {
			cond.exp = exp_copy(iter.x); //copy, since bind expressions will be freed
			vector_pushcpy(&group->condition, &cond);
		} else {
			cond.exp = NULL;
			cond.kind = iter.x->kind;
			vector_pushcpy(&group->condition, &cond);
		}

		exp_go(&iter);
	}
}

int literal_eq(expr* exp1, expr* exp2) {
	if (exp1->kind != exp2->kind) return 0;

	switch (exp1->kind) {
		case exp_num: return num_eq(*exp1->by, *exp2->by);
		default: return 0; //not a literal
	}
}

//check all conditions
int condition(substitution* sub, unsigned long i, expr* root) {
	sub_group* group = vector_get(&sub->to->groups, i);
	vector_iterator cond_iter = vector_iterate(&group->condition);
	while (vector_next(&cond_iter)) {
		sub_cond* cond = cond_iter.x;

		expr* exp = goto_idx(root, cond->idx);
		if (cond->exp) {
			if (!literal_eq(exp, cond->exp)) return 0;
		} else if (exp->kind != cond->kind) return 0; //TODO: inversion and addition expansion
	}

	return 1;
}

/// compile time condition check
int static_condition(substitution* sub, unsigned long i, expr* root) {
	sub_group* group = vector_get(&sub->to->groups, i);

	vector_iterator cond_iter = vector_iterate(&group->condition);
	while (vector_next(&cond_iter)) {
		sub_cond* cond = cond_iter.x;

		expr* exp = goto_idx(root, cond->idx);
		if (cond->exp) {
			if (!literal_eq(exp, cond->exp)) {
				sub->static_ = 0;
				return 0;
			}
		} else if (is_literal(exp) && exp->kind != cond->kind) {
			sub->static_ = 0;
			return 0;
		} else {
			sub->static_ = 0;
		}
	}

	return 1;
}

void gen_substitutes(value* val, expr* exp, unsigned int i) {
	expr_iterator iter = exp_iter(exp);
	while (!iter.done) {
		exp_get(&iter);

		if (iter.x->kind == exp_bind) {
			vector_setcpy(&val->substitutes, iter.x->bind,
										&(sub_idx) {.idx = exp_idx_copy(iter.cursor), .i=i});
		}

		exp_go(&iter);
	}
}

expr* get_sub(substitution* sub, unsigned long i) {
	sub_idx* idx = vector_get(&sub->to->substitutes, i);
	expr* root = *(expr**) vector_get(&sub->val, idx->i);
	if (!root) return 0;

	return goto_idx(root, idx->idx);
}

//substitutes in reverse in comparison to bind function
int substitute(expr* exp, substitution* sub) {
	vector_iterator group_iter = vector_iterate(&sub->to->groups);
	while (vector_next(&group_iter)) {
		sub_group* group = group_iter.x;

		if (group->condition.length > 0)
			return 0;
	}

	expr_iterator iter = exp_iter(exp);
	while (exp_next(&iter)) {
		if (iter.x->kind == exp_bind && iter.x->bind < sub->val.length) {
			get_sub(sub, iter.x->bind);
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

				printf("%s", exp->call.to->name);

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

void exp_idx_free(exp_idx* idx) {
	if (!idx) return;

	if (idx->from) exp_idx_free(idx->from);
	free(idx);
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
				vector_iterator vals = vector_iterate(&exp->call.sub);
				while (vector_next(&vals)) {
					substitution* sub = vals.x;

					vector_iterator iter = vector_iterate(&sub->val);
					while (vector_next(&iter)) {
						expr_free(*(expr**) iter.x);
					}

					vector_free(&sub->val);
				}

				vector_free(&exp->call.sub);

				break;
			}
			default:;
		}
	}


	drop(exp);
}