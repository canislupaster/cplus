// Automatically generated header.

typedef struct {
	frontend* fe;
	module* mod;

	unsigned long stack_offset; //length of sub

	map scope;
	//vector of copied substitutes for lazy evaluation
	int bind;
	vector sub;
} evaluator;
expr* ev_unqualified_access(evaluator* ev, unsigned int x);
int ev_condition(evaluator* ev, substitution* sub, unsigned long i);
int evaluate(evaluator* ev, expr* exp, expr* out);
extern char* MAIN;
