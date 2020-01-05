#include "runtime.h"

expr* ev_unqualified_access(evaluator* ev, unsigned int x) {
	expr** sub = vector_get(&ev->sub, x);
	if (sub && *sub) {
		//lazy evaluation
		if (!is_literal(*sub)) {
			expr* out = expr_new();
			if (!evaluate(ev, *sub, out)) {
				*sub = NULL;
				return NULL;
			}

			*sub = out;
		}
		return *sub;
	}

	expr* val = map_find(&ev->scope, &x);
	return val;
}

////checks equality of values
int ev_condition(evaluator* ev, substitution* sub, unsigned long i) {
	expr exp = **(expr**) vector_get(&sub->val, i);

	do {
		if (condition(sub, i, &exp)) return 1;
		evaluate(ev, &exp, &exp);
	} while (!is_literal(&exp));

	return 0;
}

////unwrap for loops and compute arithmetic, transform expr to value
int evaluate(evaluator* ev, expr* exp, expr* out) {
	out->s = exp->s;

	if (def(exp)) {
		*out = *exp;

		int ret = 0;
		if (!is_literal(exp->_for.base)) ret = 1, evaluate(ev, exp->_for.base, out->_for.base);
		if (!is_literal(exp->_for.step)) ret = 1, evaluate(ev, exp->_for.base, out->_for.step);
		if (!is_literal(exp->_for.i)) ret = 1, evaluate(ev, exp->_for.base, out->_for.i);

		if (ret) return 1;
	}

	if (exp->kind == exp_for) {

		expr i;
		if (!evaluate(ev, exp->_for.i, &i))
			return 0;
		if (i.kind != exp_num)
			return throw(&exp->_for.i->s, "expected number for loop");
		if (i.by->ty == num_decimal)
			return throw(&exp->_for.i->s, "expected integer for loop iterator");

		expr* state = exp->_for.base;
		map_insertcpy(&ev->scope, &exp->_for.x, &state);

		uint64_t int_i = i.by->uint;
		for (uint64_t iter = 0; iter < int_i - 1; iter++) {
			evaluate(ev, exp->_for.step, state);

			map_insertcpy(&ev->scope, &exp->_for.x, &state);
		}

		//evaluate last iteration to out
		evaluate(ev, exp->_for.step, out);

		map_remove(&ev->scope, &exp->_for.x);

	} else if (exp->kind == exp_cond) {

		expr i;
		if (!evaluate(ev, exp->_for.i, &i))
			return 0;

		if (i.kind != exp_num)
			return throw(&exp->_for.i->s, "expected number for condition");

		if (num_eq(*i.by, ZERO)) {
			return evaluate(ev, exp->_for.base, out);
		} else {
			return evaluate(ev, exp->_for.step, out);
		}

	} else if (exp->kind == exp_def) {

		map_insertcpy(&ev->scope, &exp->_for.x, &exp->_for.base);
		if (!evaluate(ev, exp->_for.step, out)) return 0;
		map_remove(&ev->scope, &exp->_for.x);

	} else if (exp->kind == exp_call) {

		vector_iterator vals = vector_iterate(&exp->call.sub);
		while (vector_next(&vals)) {
			substitution* sub = vals.x;

			int satisfactory = 1; //satisfies conditions

			for (unsigned long i = 0; i < sub->to->groups.length; i++) {
				if (!ev_condition(ev, sub, i)) {
					satisfactory = 0;
					break;
				}
			}

			if (!satisfactory) break;

			//if there exists an equivalent expression, insert substitutes and evaluate expression
			if (sub->to->exp) {
				//set current substitution state
				for (unsigned long i = 0; i < sub->to->substitutes.length; i++) {
					vector_pushcpy(&ev->sub, get_sub(sub, i));
				}

				ev->stack_offset += sub->to->substitutes.length;

				//copy and rename by stack offset
				*out = exp_copy_value(sub->to->exp);
				exp_rename(out, 0, ev->stack_offset);
			} else {
				//otherwise just return call as literal
				*out = exp_copy_value(exp);
			}

			return 1;
		}

		return throw(&exp->s,
								 "does not satisfy any of the callee's conditions");

	} else if (is_value(exp)) {

		switch (exp->kind) {
			case exp_bind: return evaluate(ev, ev_unqualified_access(ev, exp->bind), out);
			default: *out = exp_copy_value(exp);
				break;
		}

	} else if (unary(exp)) {

		expr inner;
		evaluate(ev, exp->inner, &inner);
		if (inner.kind != exp_num) return throw(&inner.s, "inside of unary operation is not a number");

		out->kind = exp_num;
		out->by = num_new(num_invert(*inner.by));

	} else { //binary and unary numerical operations

		expr left;
		expr right;

		evaluate(ev, exp->binary.left, &left);
		evaluate(ev, exp->binary.right, &right);

		if (left.kind != exp_num)
			return throw(&left.s,
									 "left operand is not a number, cannot be used in numerical operations");

		if (right.kind != exp_num)
			return throw(&right.s,
									 "right operand is not a number, cannot be used in numerical operations");

		num num1 = *left.by;
		num num2 = *right.by;

		switch (exp->kind) {
			case exp_add: set_num(out, num_add(num1, num2));
				break;
			case exp_mul: set_num(out, num_mul(num1, num2));
				break;
			case exp_div: set_num(out, num_div(num1, num2));
				break;
			case exp_pow: set_num(out, num_pow(num1, num2));
				break;
		}
	}

	return 1;
}

char* MAIN = "main";

void evaluate_main(frontend* fe) {
	id* main = map_find(&fe->global.ids, &MAIN);
	if (!main) return;

	vector_iterator iter = vector_iterate(&main->val);
	while (vector_next(&iter)) {
		value* val = iter.x;
		if (val->substitutes.length > 0) {
			throw(&val->s, "there must be no substitutes of main");
			continue;
		}

		evaluator ev = {.mod=&fe->global, .fe=fe, .scope=map_new(), .sub=vector_new(sizeof(expr)), .stack_offset=0};
		map_configure_ulong_key(&ev.scope, sizeof(expr*));

		expr out;
		evaluate(&ev, val->exp, &out);
		printf("main = ");
		print_expr(&out);
		printf("\n");
	}
}