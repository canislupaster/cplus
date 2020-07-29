#include <stdio.h>

#include "../src/frontend.h"
#include "../src/runtime.h"
#include "../src/colors.h"

int main(int argc, char** argv) {
	frontend fe = make_frontend();

	module_init(&fe.current);
	while (1) {
		fe.current.name = "stdin";

		set_col(stdout, WHITE);
		printf("stdin > ");

		size_t len;
		char* line = fgetln(stdin, &len);

		fe.current.s.start = line;
		fe.current.s.end = line + len;

		if (span_eq(fe.current.s, "quit")) {
			frontend_free(&fe);
			return 0;
		}

		lex(&fe);
		parse(&fe);

		if (!fe.errored) {
			evaluate_main(&fe);
		}

		//clear tokens and errored
		fe.errored = 0;

		module old_mod = fe.current;
		fe.current.tokens = vector_new(sizeof(token)); //clear tokens
	}

}
