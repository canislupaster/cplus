#include "runtime.h"

//FULLY EVALUATED?
int is_literal(expr* exp) {
	return exp->kind == exp_num;
}

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
int condition(evaluator* ev, expr* exp1, expr* exp2) {
	expr exp1_v;
	expr exp2_v;

	if (!evaluate(ev, exp1, &exp1_v) || !evaluate(ev, exp2, &exp2_v)) return 0;

	if (exp1_v.kind != exp2_v.kind) return 0;

	switch (exp1_v.kind) {
		case exp_num: return num_eq(*exp1_v.by, *exp2_v.by);
		default: return 0;
	}
}

////unwrap for loops and compute arithmetic, transform expr to value
int evaluate(evaluator* ev, expr* exp, expr* out) {
	out->s = exp->s;

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

		vector_cpy(&exp->call.sub.val, &ev->sub);

		vector_iterator iter_cond = vector_iterate(&exp->call.sub.condition);
		while (vector_next(&iter_cond)) {
			sub_cond* cond = iter_cond.x;
			if (!condition(ev, cond->what, ev_unqualified_access(ev, cond->x))) {
				//try other substitution
				//otherwise
				return throw(&cond->what->s, "does not match any substitution condition");
			}
		}

		int res = evaluate(ev, exp->call.to->val, out);
		vector_clear(&ev->sub);
		return res;

	} else if (is_value(exp)) {

		switch (exp->kind) {
			case exp_num: *out = *exp;
				break;
			case exp_bind: return evaluate(ev, ev_unqualified_access(ev, exp->bind), out);
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

	if (main->val.substitutes.length > 0) {
		throw(&main->s, "there must be no substitutes of main");
		return;
	}

	evaluator ev = {.mod=&fe->global, .fe=fe, .scope=map_new(), .sub=vector_new(sizeof(expr))};
	map_configure_ulong_key(&ev.scope, sizeof(expr*));

	expr out;
	evaluate(&ev, main->val.val, &out);
	printf("main = ");
	print_expr(&out);
	printf("\n");
}