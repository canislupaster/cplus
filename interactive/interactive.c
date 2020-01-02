#include <stdio.h>

#include "interactive.h"
#include "../src/colors.h"

int main(int argc, char** argv) {
	frontend fe = make_frontend();

	fe.file = "stdin";

	while (1) {
		set_col(stdout, WHITE);
		printf("stdin > ");

		size_t len;
		char* line = fgetln(stdin, &len);

		fe.len = (unsigned long) len;
		fe.s.start = line;
		fe.s.end = line + fe.len;

		if (span_eq(fe.s, "quit")) {
			frontend_free(&fe);
			return 0;
		}

		lex(&fe);
		parse(&fe);

		if (!fe.errored) {
			evaluate_main(&fe);
		}

		//clear tokens and errored
		vector_clear(&fe.tokens);
		fe.errored = 0;
	}
}
