// Automatically generated header.

typedef enum {
	move_left, move_right, move_inner,
	move_for_i, move_for_base, move_for_step,
	move_call_i
} move_kind;
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
typedef struct value {
	span s;
	vector groups; //conditions for substitutes in each expression
	vector substitutes; //vector of sub_idx specifying substitutes

	map substitute_idx;

	struct expr* exp;
} value;
;

expr* expr_new();
expr* expr_new_p(parser* p, expr* first);
void call_new(expr* exp, id* i);
int is_literal(expr* exp);
int is_value(expr* exp);
int binary(expr* exp);
int unary(expr* exp);
int def(expr* exp);
exp_idx* exp_idx_copy(exp_idx* from);
exp_idx* descend(exp_idx* start, move_kind kind);
exp_idx* descend_i(exp_idx* start, move_kind kind, unsigned long i, unsigned long i2);
expr* goto_idx(expr* root, exp_idx* where);
expr_iterator exp_iter(expr* exp);
void exp_ascend(expr_iterator* iter);
int exp_get(expr_iterator* iter);
void exp_go(expr_iterator* iter);
int exp_next(expr_iterator* iter);
expr exp_copy_value(expr* exp);
expr* exp_copy(expr* exp);
void exp_rename(expr* exp, unsigned threshold, unsigned offset);
void gen_condition(value* val, expr* bind_exp, unsigned int i);
int literal_eq(expr* exp1, expr* exp2);
int condition(substitution* sub, unsigned long i, expr* root);
int static_condition(substitution* sub, unsigned long i, expr* root);
void gen_substitutes(value* val, expr* exp, unsigned int i);
expr* get_sub(substitution* sub, unsigned long i);
int substitute(expr* exp, substitution* sub);
void print_expr(expr* exp);
void exp_idx_free(exp_idx* idx);
void expr_free(expr* exp);
