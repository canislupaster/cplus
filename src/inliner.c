#include "parser.c"

void inline_expr(frontend* fe, expr* e) {
    if (e->left & left_ref) {
        throw("cannot inline reference");
    } else if (e->left & left_num) {

    }
    if (e->op != op_none) {
        inline_expr(fe, e->right);

        switch (e->op) {
            case op_set:;
            case op_sub
        }
    }

}