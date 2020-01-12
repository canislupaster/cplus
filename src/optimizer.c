#include "optimizer.h"

const int CALL_COST = 10;

int cost(expr* exp) {
	if (exp->cost)
		return exp->cost;

	if (unary(exp)) {
		exp->cost = cost(exp->inner);
	} else if (binary(exp)) {
		exp->cost = cost(exp->binary.left) + cost(exp->binary.right);
	} else {
		int acc = 0;

		switch (exp->kind) {
			case exp_call: {
				exp->cost = acc + CALL_COST;
			}

			case exp_for: exp->cost = acc * cost(exp->_for.i) + cost(exp->_for.base);
				break;
			case exp_cond: exp->cost = acc + cost(exp->_for.i) + cost(exp->_for.base);
				break;
			case exp_def: exp->cost = acc + cost(exp->_for.base);
				break;

			default: exp->cost = 1;
		}
	}

	return exp->cost;
}

// simple linear optimizer
typedef struct {
	map usages;
} optimizer;

//bottom up optimizer
static void opt_reduce(expr** eref, optimizer* opt) {
	expr* exp = *eref;

	if (unary(exp)) {
		opt_reduce(&exp->inner, opt);
	} else if (binary(exp)) {
		opt_reduce(&exp->binary.left, opt);
		opt_reduce(&exp->binary.right, opt);
	}

	if (exp->kind == exp_bind) {
		map_insert(&opt->usages, &exp->bind);
	} else if (exp->kind == exp_invert && exp->inner->kind == exp_add) {
		binary_iterator iter = binary_iter(exp->inner);

		while (binary_next(&iter)) {
			if (iter.x->kind == exp_invert) {
				expr* first = iter.x->inner;
				expr* amount = iter.other;

				drop(exp);

				exp = expr_new();
				exp->kind = exp_add;
				exp->binary.left = first;
				exp->binary.right = expr_new();

				//cleanly invert num / expr
				switch (amount->kind) {
					case exp_num: {
						exp->binary.right->kind = exp_num;
						exp->binary.right->by = num_new(num_invert(*amount->by));
						drop(amount);
						break;
					}
					default: {
						exp->binary.right->kind = exp_invert;
						exp->binary.right->inner = amount;
					}
				}

				break;
			}
		}
	}

	if (exp->kind == exp_invert && exp->inner->kind == exp_num) {
		exp->kind = exp_num;
		exp->by = num_new(num_invert(*exp->inner->by));

	} else if (binary(exp) && exp->binary.left->kind == exp_num && exp->binary.right->kind == exp_num) {
		switch (exp->kind) {
			case exp_add: set_num(exp, num_add(*exp->binary.left->by, *exp->binary.right->by));
				break;
			case exp_mul: set_num(exp, num_mul(*exp->binary.left->by, *exp->binary.right->by));
				break;
			case exp_div: set_num(exp, num_div(*exp->binary.left->by, *exp->binary.right->by));
				break;
			case exp_pow: set_num(exp, num_pow(*exp->binary.left->by, *exp->binary.right->by));
				break;
		}
	}

	if (exp->kind == exp_mul || exp->kind == exp_div) {
		remove_num(eref, &ONE);
		exp = *eref;
	} else if (exp->kind == exp_add) {
		remove_num(eref, &ZERO);
		exp = *eref;
	}

	//TODO: simplify to def if i is 1
	switch (exp->kind) {
		case exp_for: {
			opt_reduce(&exp->_for.step, opt);
			int unused = !exp->_for.named || !map_find(&opt->usages, &exp->_for.x);

			opt_reduce(&exp->_for.base, opt);
			opt_reduce(&exp->_for.i, opt);

			if (exp->_for.i->kind == exp_num && num_eq(*exp->_for.i->by, ONE)) {
				if (unused) {
					expr_free(exp->_for.base);

					expr* step = exp->_for.step;
					drop(exp);

					exp = step; //i is one and x/base is not used, return step
				}

				exp->kind = exp_def;
			} else if (unused) {
				exp->kind = exp_cond; //expression can be simplified to a conditional if step (first) does not use x
			} else if (exp->_for.step->kind == exp_add) {
				//reduce repeated addition to multiplication
				expr* by = extract_operand(exp->_for.step, exp->_for.x);
				if (!by)
					break;

				expr* i = exp->_for.i;
				expr* base = exp->_for.base;
				drop(exp);

				expr* mult = expr_new();
				mult->kind = exp_mul;

				mult->binary.left = expr_new();
				mult->binary.left = by;
				mult->binary.right = i;

				exp = expr_new();
				exp->kind = exp_add;
				exp->binary.left = mult;
				exp->binary.right = base;

				*eref = exp;

				return opt_reduce(eref, opt);
			} else if (exp->kind == exp_mul) {
				//reduce repeated multiplication to exponentiation
				expr* by = extract_operand(exp, exp->_for.x);
				if (!by)
					break;

				expr* i = exp->_for.i;
				expr* base = exp->_for.base;
				drop(exp);

				expr* powd = expr_new();
				powd->kind = exp_pow;
				powd->binary.left = by;
				powd->binary.right = i;

				exp = expr_new();
				exp->kind = exp_mul;
				exp->binary.left = powd;
				exp->binary.right = base;

				*eref = exp;

				return opt_reduce(eref, opt);
			}

			break;
		}
		case exp_call: {
			vector_iterator vals = vector_iterate(&exp->call.sub);
			while (vector_next(&vals)) {
				vector_iterator subs = vector_iterate(&((substitution*) vals.x)->val);
				while (vector_next(&subs)) {
					opt_reduce(subs.x, opt);
				}
			}

			break;
		}

		default:;
	}

	*eref = exp;
}

void reduce(expr** exp) {
	optimizer opt = {.usages=map_new()};
	map_configure_ulong_key(&opt.usages, 0);
	opt_reduce(exp, &opt);

	map_free(&opt.usages);
}