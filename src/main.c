#include "stdio.h"

#include "main.h"

//im using main mostly for temporary parsing tests at this point, like so:
//im just gonna commit a bunch of checkpoints dont mind me
int main(int argc, char** argv) {
  frontend fe = make_frontend("test.c+");

  lex(&fe);
//  vector_iterator iter = vector_iterate(&fe.tokens);
//  while (vector_next(&iter)) {
//	char* s = spanstr(&((token*)iter.x)->s);
//	printf(" %s ", s);
//  }
  parse(&fe);

  if (!fe.errored)
	print_module(&fe.global);

  frontend_free(&fe);

  evaluate_main(&fe.global);

  return 0;
}