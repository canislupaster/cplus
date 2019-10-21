#include "stdio.h"

#include "frontend.c"

int main(int argc, char** argv) {
    frontend fe = make_frontend("test.c+");

    expr x;
    if (!parse_expr(&fe, &x, 0)) throw(&fe, NULL, "aaaa");

    print_expr(&x);

    return 0;
}