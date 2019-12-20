#include "frontend.c"

typedef struct {
    frontend* fe;
    token current;

    unsigned long pos;

    module* mod;
    map* substitute_idx;
    vector reducers;
} parser;

int throw_here(parser* p, const char* x) {
    return throw(&p->current.s, x);
}

/// doesn't return null
void parse_next(parser* p) {
    p->current = *(token*)vector_get(&p->fe->tokens, p->pos);
    if (p->current.tt != t_eof) p->pos++;
}

/// doesn't return null
token* parse_peek(parser* p) {
    return vector_get(&p->fe->tokens, p->pos);
}

token_type parse_peek_x(parser* p, int x) {
    token* t = vector_get(&p->fe->tokens, p->pos+x-1);
    return t ? t->tt : t_eof;
}

/// returns 0 if not matched, otherwise sets p.x
int parse_next_eq(parser* p, token_type tt) {
    token* t = parse_peek(p);
    if (t->tt == tt) {
        p->pos++;
        p->current = *t;
        return 1;
    } else {
        return 0;
    }
}

int separator(parser* p) {
    token* t = parse_peek(p);
    return t->tt == t_eof || t->tt==t_sync;
}

void synchronize(parser* p) {
    while (!separator(p)) parse_next(p);
}

int parse_expr(parser* p, expr* x_expr, int bind, unsigned op_prec); //forward decl

void parse_detach(expr* x_expr) {
    expr* detached = heapcpy(sizeof(expr), x_expr);

    x_expr->left.kind = left_expr;
    x_expr->left.val.expr = detached;
}

void module_init(module* b) {
    b->ids = map_new();
    map_configure_string_key(&b->ids, sizeof(id*));
}
//
//value value_new(expr* e) {
//    value v = {.substitutes=vector_new(sizeof(expr)), .val=e}; //TODO: REDUCTION
//    return v;
//}
//
//value from_left(expr* e) {
//    value v = value_new(e);
//    return v;
//}
//
//int resolve_i(expr* i) {
//
//}
//
// //FIRST: number from i and derivative from current
// int resolve_for(for_expr* fore) {
//
// }
//
//int value_eq(parser* p, value* x1, value* x2) {
//    if (x1->substitutes.length != x2->substitutes.length) return 0;
//    vector_iterator iter = vector_iterate(&x1->substitutes);
//
//    while (vector_next(&iter)) {
//        if (!expr_eq(iter.current, vector_get(&x2->substitutes, iter.i-1))) return 0;
//    }
//}

int expr_eq(value* v1, value* v2, expr* e1, expr* e2, int nocompare);

int fore_eq(for_expr* fore1, for_expr* fore2, int nocompare) {
    return expr_eq(NULL, NULL, &fore1->i, &fore2->i, nocompare)
            && expr_eq(NULL, NULL, &fore1->base, &fore2->base, nocompare)
            && expr_eq(NULL, NULL, &fore1->x, &fore2->x, nocompare + 1);
}

int id_eq(id* id1, id* id2, int nocompare) {
    if (id1 == id2) return 1;

    if (id1->val.substitutes.length != id2->val.substitutes.length) return 0;

    vector_iterator iter = vector_iterate(&id1->val.substitutes);
    vector_iterator iter2 = vector_iterate(&id2->val.substitutes);

    while (vector_next(&iter)) {
        vector_next(&iter2);
        if (!expr_eq(&id1->val, &id2->val, iter.x, iter2.x, nocompare))
            return 0;
    }

    return expr_eq(NULL, NULL, id1->val.val, id2->val.val, nocompare);
}

int applied_eq(expr* e1, expr* e2, int nocompare) {

}

int expr_eq(value* v1, value* v2, expr* e1, expr* e2, int nocompare) {
    if (e1 == e2) return 1;
    if (e1->left!=e2->left || e1->flags!=e2->flags || e1->substitutes.length != e2->substitutes.length) return 0;

    switch (e1->left) {
        case left_str: return strcmp(e1->val.str, e2->val.str) == 0;
        case left_num: return e1->val.num->ty == e2->val.num->ty && e1->val.num->uint == e2->val.num->uint;
        case left_expr: return expr_eq(NULL, NULL, e1, e2, nocompare);
        case left_for: return fore_eq(e1->val.fore, e2->val.fore, nocompare - 1);
        case left_access: {
            if (e1->val.access->res != e2->val.access->res) return 0;

            switch (e1->val.access->res) {
                case a_unbound: return 1;
                case a_id: return id_eq(e1->val.access->val.id, e2->val.access->val.id, nocompare);
                case a_for: {
                    if (!nocompare) return fore_eq(e1->val.access->val.fore, e1->val.access->val.fore, nocompare + 1);
                }
                case a_sub: {
                    return e1->val.access->val.idx == e2->val.access->val.idx;
                }
            }
        }
        case left_bind: {
            // compare if both substitute positions are equal
            return e1->val.bind == e2->val.bind;
        }
    }
}

int bind(substitution* s, expr* be, expr* e) {
    if (e->flags!=be->flags) return 0;
    if (be->apply != unapplied) {
        if (e->apply == unapplied) return 0;
        if (be->apply == apply_bind) {
            vector_pushcpy(&s->substitutions, &e->applier.id->val);
        } else if (be->apply == applied) {
            if (!id_eq(e->applier.id, be->applier.id) && !expr_eq()) return 0;

            vector_iterator biter = vector_iterate(&be->substitutes);
            vector_iterator eiter = vector_iterate(&e->substitutes);

            while (vector_next(&biter) && vector_next(&eiter)) {
                if (!bind(s, biter.x, eiter.x)) return 0;
            }
        }
    }

    if (be->left == left_bind) {
        value left = from_left(e);
        vector_pushcpy(&s->substitutions, &left);

        return 1;
    } else {
        if (e->left!=be->left) return 0;
        switch (e->left) {
            case left_expr: return bind(s, be->val.expr, e->val.expr);
            default: return expr_eq(NULL, NULL, e, be, 0);
        }
    }
}

void commute(num* num1, num* num2) {
    if (num1->ty != num2->ty) { //ensure same precision, assume two's complement
        if (num1->ty == num_decimal) {
            num2->ty = num_decimal;
            num2->decimal = (double) num2->uint;
        } else if (num2->ty == num_decimal) {
            num1->ty = num_decimal;
            num1->decimal = (double) num1->uint;
        }
    }
}

num ZERO = {.ty = num_integer, .integer = 0};
num ONE = {.ty = num_integer, .integer = 1};

num mul(num num1, num num2) {
    commute(&num1, &num2);

    num res = {.ty = num1.ty};
    if (num1.ty == num_decimal) {
        res.decimal = num1.decimal * num2.decimal;
    } else {
        res.uint = num1.uint * num2.uint;
    }

    return res;
}

num add(num num1, num num2) {
    commute(&num1, &num2);

    num res = {.ty = num1.ty};
    if (num1.ty == num_decimal) {
        res.decimal = num1.decimal + num2.decimal;
    } else {
        res.uint = num1.uint + num2.uint;
    }

    return res;
}

typedef struct {

};

const simple SIMPLE_ONE = {.kind=simple_num, .val = {.by = &ONE}};
const simple SIMPLE_ZERO = {.kind=simple_num, .val = {.by = &ZERO}};

//TODO: fix mem leaks hehe
simple* gradient(simple* step, unsigned long x) {
    switch (step->kind) {
        //cancel constant operations
        case simple_invert:
        case simple_add: {
            return step->first ? gradient(step->first, x) : heapcpy(sizeof(simple), &SIMPLE_ZERO);
        }
        case simple_bind: {
            if (step->val.bind == x) {
                return step->first ? step->first : heapcpy(sizeof(simple), &SIMPLE_ONE);
            }
        }
        //keep forms of multiplication
        default: {
            simple* res = heap(sizeof(simple));
            *res = *step;
            res->first = gradient(step->first, x);

            return res;
        }
    }
}

int substitute(simple* exp, substitution* sub) {
    if (exp->first) substitute(exp->first, sub);

    if (exp->kind == simple_bind) {
        exp->kind = simple_inner;
        exp->val.inner = *(simple**)vector_get(&sub->substitutions, sub->substitutions.length);
        vector_pop(&sub->substitutions);
    } else if (exp->kind == simple_inner) {
        substitute(exp->val.inner, sub);
    }
}

void reduce(expr* e, simple* s) {
    if (e->apply == applied) {
        substitute(&e->applier.id->val.val, &e->substitutes);
    }

    switch (e->left) {
        case left_for: {
            s->first = gradient(reduce(e->val.fore->x));

            s->kind = simple_num;
            s->val.by
        }
    }
}

int try_parse_name(parser* p) {
    token* x = parse_peek(p);
    if (is_name(x)) {
        parse_next(p);
        return 1;
    } else {
        return 0;
    }
}

int try_parse_unqualified(parser* p) {
    token* x = parse_peek(p);
    if (is_name(x) && !x->val.name->qualifier) {
        parse_next(p); return 1;
    } else {
        return 0;
    }
}

int parse_id(parser* p) {
    id xid;
    xid.s.start = parse_peek(p)->s.start;

    //reset reducers
    p->reducers = vector_new(sizeof(for_expr*));

    xid.val.substitutes = vector_new(sizeof(expr));
    xid.val.substitute_idx = map_new();

    //maps binds name to index for unbiased comparison of ids
    map_configure_string_key(&xid.val.substitute_idx, sizeof(unsigned long));

    p->substitute_idx = &xid.val.substitute_idx;

    //start parsing substitutes
    xid.substitutes.start = parse_peek(p)->s.start;

    if (parse_peek_x(p, 2) != t_eq) {
        expr e;
        if (parse_expr(p, &e, 1, 1)) {
            vector_pushcpy(&xid.val.substitutes, &e);
        } else {
            return throw_here(p, "expected substitute");
        }
    }
    
    if (try_parse_unqualified(p)) {
        xid.name = p->current.val.name->x;
    } else {
        return throw_here(p, "expected name for identifier");
    }

    while (!parse_next_eq(p, t_eq)) {
        expr e;
        if (parse_expr(p, &e, 1, 1)) {
            vector_pushcpy(&xid.val.substitutes, &e);
        } else {
            return throw_here(p, "expected = or substitute");
        }
    }

    xid.substitutes.end = p->current.s.start;
    span eq = p->current.s;

    xid.val.val = heap(sizeof(expr));
    if (!parse_expr(p, xid.val.val, 0, 0)) {
        synchronize(p); throw(&eq, "expected value");
    }

    id* heap_id = heapcpy(sizeof(xid), &xid);
    map_insertcpy(&p->mod->ids, &xid.name, &heap_id);

    xid.s.end = p->current.s.end;
    return 1;
}

int parse_mod(parser* p, module* b) {
    module_init(b);
    module* old_b = p->mod;
    p->mod = b;

    while (!parse_next_eq(p, t_eof)) {
        parse_id(p);

        if (!separator(p)) {
            synchronize(p);
            throw_here(p, "expected end of identifier (newline without indentation)");
        }

        parse_next_eq(p, t_sync);
    }

    p->mod = old_b;
    return 1;
}

void parse(frontend* fe) {
    parser p = {fe, .pos=0};

    module_init(&p.fe->global);
    p.mod = &p.fe->global;

    parse_mod(&p, &p.fe->global);
}

access unqualified_access(parser* p, char* x) {
    access a;

    vector_iterator for_iter = vector_iterate(&p->reducers);
    while (vector_next(&for_iter)) {
        for_expr** fore = for_iter.x;

        if (strcmp((*fore)->name, x) == 0) {
            a.res = a_for;
            a.val.fore = *fore;
            return a;
        }
    }

    unsigned long* idx = map_find(p->substitute_idx, &x);
    if (idx) {
        a.res = a_sub;
        a.val.idx = *idx;
        return a;
    }

    id* xid = map_find(&p->mod->ids, &x);
    if (xid) {
        a.res = a_id;
        a.val.id = xid;
        return a;
    }

    a.res = a_unbound;
    return a;
}

access qualified_access(parser* p, name* n) {
    if (!n->qualifier) {
        return unqualified_access(p, n->x);
    }

    //TODO: qualified ids

    access a;
    a.res = a_unbound;
    return a;
}

//i was going to use this but now it just makes the code harder to read
typedef enum {left_fail=0, left_fallthrough, left_parsed} left_parse_result;

left_parse_result parse_left_expr(parser* p, left* left, int bind) {
    left->flags=0;

    if (parse_next_eq(p, t_sub)) left->flags |= left_neg;
    if (parse_next_eq(p, t_add)) left->flags |= left_add;

    if (parse_next_eq(p, t_for)) {
        for_expr* fore = heap(sizeof(for_expr));
        parse_expr(p, &fore->i, 0, 1);
        
        if (!try_parse_unqualified(p)) {
            free(fore);
            return throw_here(p, "expected name for identifier of for expression");
        }

        fore->name = p->current.val.name->x;

        if (parse_next_eq(p, t_eq)) {
            if (!parse_expr(p, &fore->base, 0, 1)) {
                free(fore); return throw_here(p, "expected base expression");
            }
        } else {
            fore->base.left = left_access;
            fore->base.apply = unapplied;

            access a = unqualified_access(p, fore->name);
            if (a.res == a_unbound) {
                free(fore); return throw_here(p, "implicit reference in base of for expression is not defined");
            }

            fore->base.val.access = heapcpy(sizeof(access), &a);
        }

        vector_pushcpy(&p->reducers, &fore);

        if (!parse_expr(p, &fore->x, 0, 1)) {
            free(fore);
            return left_fallthrough;
        }

        vector_pop(&p->reducers);

        x_expr->left = left_for;
        x_expr->val.fore = fore;

        return left_parsed;
    }

    if (parse_next_eq(p, t_num)) {
        x_expr->left = left_num;
        x_expr->val.num = p->current.val.num;
    } else if (parse_next_eq(p, t_str)) {
        x_expr->left = left_str;
        x_expr->val.str = p->current.val.str;
    } else if (try_parse_name(p)) {
        if (bind && !p->current.val.name->qualifier) {
            x_expr->left = left_bind;
            map_insertcpy(p->substitute_idx, &p->current.val.name->x, &p->substitute_idx->length);
            x_expr->val.bind = p->substitute_idx->length; //set value to index

            return left_parsed;
        }

        access a = qualified_access(p, p->current.val.name);

        x_expr->left = left_access;
        x_expr->val.access = heapcpy(sizeof(access), &a);

        if (a.res == a_unbound) throw_here(p, "name does not reference an identifier, reducer, or substitute");
    } else {
        return left_fallthrough; //not an expression, while zero is failure
    }

    return left_parsed;
}

int parse_expr(parser* p, expr* x_expr, int bind, unsigned op_prec) {
    int lparen = 0;
    if (parse_next_eq(p, t_paren)) {
        //parentheses around expression, set base precedence to zero
        op_prec = 0;
        lparen = 1;
    }

    x_expr->left.span.start = parse_peek(p)->s.start;
    x_expr->substitutes = vector_new(sizeof(expr));

    left_parse_result left = parse_left_expr(p, x_expr, bind);

    if (left==left_fail || left==left_fallthrough) return 0;

    x_expr->left.span.end = p->current.s.end;
    x_expr->apply = unapplied;

    //parse ops
    while(1) {
        if (parse_next_eq(p, t_paren)) {
            if (lparen) {
                lparen = 0;
                x_expr->left.span.end = p->current.s.end; //update span end

                continue;
            } else {
                return 1;
            }
        }

        token* tok = parse_peek(p);
        if (!is_name(tok)) { //no op
            break;
        }

        id* applier = map_find(&p->mod->ids, &tok->val.name->x); //TODO: qualified appliers
        unsigned int prec=0;

        if (applier) {
            prec = applier->precedence;

            x_expr->apply = applied;
            x_expr->applier.id = applier;
        } else if (bind) {
            if (!lparen) return 1; //probably an identifier definition, not ours to bind to

            x_expr->apply = apply_bind;
            x_expr->applier.bind = tok->val.name;
        } else {
            // error depending on binding strength
            // this allows things like for loop quantities to return after parsing but identifiers to require parsing
            if (0 < op_prec) return 1;
            else return throw_here(p, "undefined identifier");
        }
    
        //dont parse any more
        if (prec < op_prec) break;
        else parse_next(p); //otherwise increment parser

        //applied already, detach
        //happens every time after first loop since each loop sets applied
        if (x_expr->apply != unapplied) {
            parse_detach(x_expr);
        }

        if (x_expr->apply == apply_bind) {
            while (1) {
                expr* sub = vector_push(&x_expr->substitutes);

                if (!parse_expr(p, sub, bind, prec)) {
                    vector_pop(&x_expr->substitutes);
                    break;
                }
            }
        } else {
            unsigned subs = applier->val.substitutes.length;

            for (unsigned i=0; i<subs; i++) {
                expr* sub = vector_push(&x_expr->substitutes);
                parse_expr(p, sub, bind, prec);
            }

            if (x_expr->substitutes.length != applier->val.substitutes.length) {
                throw_here(p, isprintf("expected %lu substitutes, got %lu", x_expr->substitutes.length, applier->val.substitutes));
                note(&applier->substitutes, "defined here");
            }
        }
    }

    return 1;
}

void print_name(name* n) {
    if (n->qualifier) printf("%s.", n->qualifier);
    printf("%s", n->x);
}

void print_expr(expr* e) {
    if (e->flags & left_neg) printf("-");
    if (e->flags & left_add) printf("+");

    switch (e->left) {
        case left_num: print_num((num*)e->val.num); break;
        case left_str: printf("\"%s\"", (char*)e->val.str); break;
        case left_expr: {
            printf("("); print_expr((expr*)e->val.expr); printf(")");
            break;
        }
        case left_bind: printf("%s", (char*)e->val.bind); break;
        case left_access: {
            switch (e->val.access->res) {
                case a_unbound: printf("(unbound)");
                case a_for: printf("%s", e->val.access->val.fore->name);
                case a_sub: printf("(sub %lu)", e->val.access->val.idx);
                case a_id: printf("%s", e->val.access->val.id->name);
            }

            break;
        }
        case left_for: {
            printf("for ");
            print_expr(&e->val.fore->i); printf(" ");
            printf("%s", e->val.fore->name); printf(" ");
            print_expr(&e->val.fore->base); printf(" ");
            print_expr(&e->val.fore->x);

            break;
        }
    }

    if (e->apply == apply_bind) {
        printf(" "); print_name(e->applier.bind); printf(" ");
    } else if (e->apply == applied) {
        printf(" "); printf("%s", e->applier.id->name); printf(" ");
    } else {
        return;
    }

    vector_iterator iter = vector_iterate(&e->substitutes);
    while(vector_next(&iter)) {
        print_expr(iter.x); printf(" ");
    }
}

void print_module(module* b) {
    map_iterator x = map_iterate(&b->ids);
    while (map_next(&x)) {
        id* xid = *(id**)x.x;
        printf("%s ", xid->name);
        
        vector_iterator iter = vector_iterate(&xid->val.substitutes);
        while(vector_next(&iter)) {
            print_expr(iter.x); printf(" ");
        }

        printf("= ");

        print_expr(xid->val.val);

        printf("\n");
    }
}