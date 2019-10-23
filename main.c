#include "stdio.h"

#include "frontend.c"

//im using main mostly for temporary parsing tests at this point, like so:
//im just gonna commit a bunch of checkpoints dont mind me
int main(int argc, char** argv) {
    frontend fe = make_frontend("test.c+");

    lex(&fe);

    parser p = {.fe=&fe, .pos=0};
    expr x;
    int res = parse_expr(&p, &x, 0);

    print_expr(&x);

    return 0;
}