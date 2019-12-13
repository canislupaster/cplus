#include "frontend.c"

typedef struct {
    frontend* fe;
    token x;

    unsigned long pos;

    block* current;
} parser;

int throw_here(parser* p, const char* x) {
    throw(p->fe, &p->x.s, x);
}

/// doesn't return null
void parse_next(parser* p) {
    p->x = *(token*)vector_get(&p->fe->tokens, p->pos);
    if (p->x.tt != t_eof) p->pos++;
}

/// doesn't return null
token* parse_peek(parser* p) {
    return vector_get(&p->fe->tokens, p->pos);
}

/// returns 0 if not matched, otherwise sets p.x
int parse_next_eq(parser* p, token_type tt) {
    token* t = parse_peek(p);
    if (t->tt == tt) {
        p->pos++;
        p->x = *t;
        return 1;
    } else {
        return 0;
    }
}

//for debugging convenience
int separator(parser* p) {
    token* t = parse_peek(p);
    return t->tt==t_sep || t->tt == t_comma || t->tt==t_rbrace || t->tt==t_rparen || t->tt==t_ridx;
}

void synchronize(parser* p) {
    do parse_next(p); while (!separator(p));
}

typedef enum {
    op_none,
    op_sub, op_add, op_mul, op_div, op_idx,
    op_set, op_cast
} op;

typedef enum {
    left_access,
    left_call,

    left_num,
    left_char,
    left_str,

    left_expr
} left;

typedef enum {
    //prefix ops
    left_neg = 0x1,
    left_ref = 0x2,
    left_cast = 0x4,
    //prefix incr/decr: ++x
    left_incr = 0x8,
    left_decr = 0x10,
    //affix ops: x++
    left_incr_after = 0x20,
    left_decr_after = 0x40,
} left_flags;

const int left_num_op = left_incr | left_incr_after | left_decr | left_decr_after | left_neg;

/// linked list
typedef struct s_expr {
    op op;
    /// assign after op
    char assign;

    left left;
    left_flags flags;

    span span;
    void* x;
    //always expr
    struct s_expr* right;
} expr;

typedef struct {
    id_type ty;
    char* name;
    char uninitialized;
    block val;
} bind_stmt;

typedef enum {
    fn_arg_restrict = 0x01
} fn_arg_flags;

typedef struct {
    id_type ty;
    char* name;
    fn_arg_flags flags;
} fn_arg;

typedef struct {
    id_type ty;
    /// vector of fn_args
    vector args;
    char* name;

    char extern_fn;
    block val;
} fn_stmt;

typedef struct {
    fn_stmt* target;
    char* name;
    /// vector of expr
    vector args;
} fn_call;

int try_parse_expr(parser* p, expr* x_expr, unsigned op_prec); //forward decl

expr* try_parse_new_expr(parser* p) {
    expr* e = malloc(sizeof(expr));
    if (!try_parse_expr(p, e, 0)) {
        free(e);
        return NULL;
    }

    return e;
}

expr* parse_idx(parser* p) {
    if (parse_next_eq(p, t_lidx)) {
        span* l_idx = &p->x.s; //record starting bracket

        expr* i_expr = try_parse_new_expr(p);
        if (!i_expr) {
            throw_here(p, "expected index");
            return NULL;
        }

        if (!parse_next_eq(p, t_ridx)) {
            throw_here(p, "expected right bracket to end index");
            note(p->fe, l_idx, "other bracket here");
        }

        return i_expr;
    } else {
        return NULL;
    }
}

int parse_length(parser* p, unsigned long* x) {
    if (parse_next_eq(p, t_lidx)) {
        span* l_idx = &p->x.s; //record starting bracket

        if (parse_next_eq(p, t_num)) {
            num* n = p->x.val;
            if (n->ty != num_integer) return throw_here(p, "expected integer");
            *x = n->uint;
        }

        if (!parse_next_eq(p, t_ridx)) {
            throw_here(p, "expected right bracket to end length");
            note(p->fe, l_idx, "other bracket here");
        }

        return 1;
    } else {
        return 0;
    }
}

/// parse anything that can be a type attribution, to be resolved later
int parse_id_type(parser* p, id_type* tid) {
    tid->resolved = 0;
    tid->td.flags = 0;

    tid->s.start = parse_peek(p)->s.start;

    if (parse_next_eq(p, t_const)) tid->td.flags |= ty_const;
    if (!parse_next_eq(p, t_id)) return 0;

    tid->name = p->x.val;

    if (parse_length(p, &tid->td.size)) //try parse array size
        tid->td.flags |= ty_arr;

    if (parse_next_eq(p, t_mul))
        tid->td.flags |= ty_ptr;

    tid->s.end = p->x.s.end;

    return 1;
}

int is_id(id_type* tid) {
    return !tid->td.flags;
}

void parse_set_left(expr* expr, left l, void* x) {
    expr->left = l;
    expr->x = x;
}

void parse_detach(expr* x_expr) {
    expr* detached = malloc(sizeof(expr));
    memcpy(detached, x_expr, sizeof(expr));

    x_expr->left = left_expr;
    x_expr->x = detached;
}

void stmt_free(stmt* s);

void block_init(block* b) {
    b->stmts = vector_new(sizeof(stmt));

    b->declarations = map_new();
    map_configure_string_key(&b->declarations, sizeof(stmt));
}

void block_free(block* b) {
    vector_iterator iter = vector_iterate(&b->stmts);
    while (vector_next(&iter)) {
        stmt_free(iter.x);
    }

    vector_free(&b->stmts);
    map_free(&b->declarations);

    free(b);
}

int parse_separated(parser* p, token_type sep, token_type end) {
    if (parse_next_eq(p, end)) return 1;

    if (!parse_next_eq(p, sep)) {
        throw_here(p, "expected separator");
    }

    return 0;
}

int parse_stmt_end(parser* p) {
    //any separator works, but only semicolon is parsed
    if (!separator(p)) throw_here(p, "expected semicolon after statement");
    return parse_next_eq(p, t_sep);
}

void fn_free(fn_stmt* fn) {
    vector_free(&fn->args); block_free(&fn->val); free(fn);
}

void print_block(block* b);
int parse_block(parser* p, block* b);

/// parses bind or set, returns name of binding
int try_parse_bind_fn(parser* p) {
    id_type tid;

    if (parse_id_type(p, &tid) && parse_next_eq(p, t_id)) {
        stmt s;
        char* target;

        s.s.start = tid.s.start;
        target = p->x.val; //name of binding

        if (parse_next_eq(p, t_lparen)) {
            //parse fn
            fn_stmt* fn = malloc(sizeof(fn_stmt));
            span fn_paren = p->x.s;

            fn->ty = tid;
            fn->name = target;
            fn->args = vector_new(sizeof(fn_arg));
            block_init(&fn->val);

            //parse arguments
            while (!parse_next_eq(p, t_rparen)) {
                if (separator(p)) {
                    fn_free(fn);

                    throw_here(p, "expected matching paren for function declaration");
                    note(p->fe, &fn_paren, "other paren here");
                    return 1; //consume
                }

                fn_arg* arg = vector_push(&fn->args);

                if (!parse_id_type(p, &arg->ty)) {
                    throw_here(p, "expected argument type");
                    synchronize(p);
                }

                if (!parse_next_eq(p, t_id)) {
                    throw_here(p, "expected argument name");
                    synchronize(p);
                }

                arg->name = p->x.val;

                stmt* fn_arg_s = vector_push(&fn->val.stmts);
                fn_arg_s->t = s_fn_arg; fn_arg_s->s=p->x.s; fn_arg_s->x = arg;

                scope_insert(p->fe, &fn->val.declarations, arg->name, fn_arg_s);

                if (parse_separated(p, t_comma, t_rparen)) break;
            }

            s.s.end = p->x.s.end; //span only includes function header

            if (!parse_block(p, &fn->val)) {
                fn->extern_fn=1;
            } else {
                fn->extern_fn=0;
            }

            s.t=s_fn;
            s.x=fn;

            scope_insert(p->fe, &p->current->declarations, target, &s);
        } else {
            bind_stmt* bind = malloc(sizeof(bind_stmt));
            bind->name = target;
            bind->ty = tid;

            if (parse_next_eq(p, t_set)) {
                if (!parse_block(p, &bind->val)) {
                    throw_here(p, "expected block or expression in binding");
                    bind->uninitialized=1;
                } else {
                    bind->uninitialized=0;
                }
            } else {
                bind->uninitialized=1;
            }

            parse_stmt_end(p);

            s.t=s_bind;
            s.x=bind;
            s.s.end = p->x.s.end;
        }

        vector_pushcpy(&p->current->stmts, &s);
        return 1;
    } else {
        return 0;
    }
}

int parse_stmt(parser* p) {
    parser peek_parser = *p; //TODO: make cleaner peek procedure mechanisms

    if (try_parse_bind_fn(&peek_parser)) {
        *p = peek_parser; //update pos
        return 1;
    }

    stmt new_s;
    int ret = parse_next_eq(p, t_return);

    expr* e = try_parse_new_expr(p);
    if (e) {
        if (!parse_stmt_end(p)) ret=1; //if there isn't a semicolon, it shall be qualified as a return statement, which can later be validated

        new_s.t = ret ? s_ret : s_expr;
        new_s.x = e;

        new_s.s.start=e->span.start;
        new_s.s.end=p->x.s.end;

        vector_pushcpy(&p->current->stmts, &new_s);
        return 1;
    }

    throw_here(p, "expected statement/expression");
    return 0;
}

int try_parse_block_inner(parser* p) {
    if (parse_next_eq(p, t_lbrace)) {
        span l_brace = p->x.s;

        while (!parse_next_eq(p, t_rbrace)) {
            if (separator(p)) {
                throw_here(p, "expected matching brace for block");
                note(p->fe, &l_brace, "other brace here");

                return 1; //consume
            }

            if (!parse_stmt(p)) {
                synchronize(p);
            }
        }

        return 1;
    } else {
        return 0;
    }
}

//code duplication abound

int parse_block_braced(parser* p, block* b) {
    block_init(b);
    block* old_b = p->current;
    p->current = b;

    int ret = try_parse_block_inner(p);

    p->current = old_b;

    return ret;
}

int parse_block(parser* p, block* b) {
    if (parse_next_eq(p, t_sep)) return 0;

    block_init(b);
    block* old_b = p->current;
    p->current = b;

    if (!try_parse_block_inner(p)) {
        //try to parse expression as return statement

        stmt s = {.t=s_ret};
        s.s.start = parse_peek(p)->s.start;

        expr* x = try_parse_new_expr(p);
        if (!x) {
            throw_here(p, "expected expression");
            p->current = old_b;
            return 0;
        }

        s.x = x;
        s.s.end = p->x.s.end;
        vector_pushcpy(&b->stmts, &s);
    }

    p->current = old_b;
    return 1;
}

void parse(frontend* fe) {
    parser p = {fe, .pos=0};

    block_init(&p.fe->global);

    p.current = &p.fe->global;

    while (parse_peek(&p)->tt!=t_eof && parse_stmt(&p));
}

int try_parse_left_access(parser* p, expr* x_expr) {
    id_type* tid = malloc(sizeof(id_type));
    if (parse_id_type(p, tid)) {
        if (is_id(tid) && parse_next_eq(p, t_lparen)) {
            span fn_paren = p->x.s;

            fn_call* fc = malloc(sizeof(fn_call));
            fc->args = vector_new(sizeof(expr));
            fc->name = tid->name;
            fc->target = NULL;

            //parse arguments
            while (!parse_next_eq(p, t_rparen)) {
                if (separator(p)) {
                    free(fc); vector_free(&fc->args);

                    throw_here(p, "expected matching paren for function call");
                    note(p->fe, &fn_paren, "other paren here");
                    return 0;
                }

                expr* expr_arg = vector_push(&fc->args);
                if (!try_parse_expr(p, expr_arg, 0)) { //parse expr in place
                    throw_here(p, "expected argument"); //throw a descriptive error
                    synchronize(p);
                }

                if (parse_separated(p, t_comma, t_rparen)) break;
            }

            parse_set_left(x_expr, left_call, fc);
        } else {
            expr* r_expr = parse_idx(p);

            if (r_expr) {
                //set left to index expr
                expr* index_expr = malloc(sizeof(expr));
                parse_set_left(index_expr, left_access, tid);
                index_expr->right=r_expr;

                parse_set_left(x_expr, left_expr, index_expr);
            } else {
                //not call or index, simple access
                parse_set_left(x_expr, left_access, tid);
            }
        }

        return 1;
    } else {
        return -1;
    }
}

int try_parse_left_expr(parser* p, expr* x_expr) {
    x_expr->flags=0;

    if (parse_next_eq(p, t_ref)) {
        x_expr->flags |= left_ref;
    }

    if (parse_next_eq(p, t_sub)) x_expr->flags |= left_neg;
    if (parse_next_eq(p, t_incr)) x_expr->flags |= left_incr;
    if (parse_next_eq(p, t_decr)) x_expr->flags |= left_decr;

    if (parse_next_eq(p, t_num)) {
        //copy to left
        num* x = p->x.val;
        parse_set_left(x_expr, left_num, x);
    } else if (parse_next_eq(p, t_char)) {
        char* c = p->x.val;
        parse_set_left(x_expr, left_char, c);
    } else if (parse_next_eq(p, t_str)) {
        char* str = p->x.val;
        parse_set_left(x_expr, left_str, str);
    } else {
        return try_parse_left_access(p, x_expr);
    }

    if (parse_next_eq(p, t_incr)) x_expr->flags |= left_incr_after;
    if (parse_next_eq(p, t_decr)) x_expr->flags |= left_decr_after;

    return 1;
}

int try_parse_expr(parser* p, expr* x_expr, unsigned op_prec) {
    int left=0; //error in the left side: return the right side instead
    span* l_paren=NULL;

    if (parse_next_eq(p, t_lparen)) {
        //parentheses around expression, set base precedence to zero
        l_paren = &p->x.s;
        op_prec = 0;
    }

    x_expr->span.start = parse_peek(p)->s.start;
    //parse left side and set left status
    left = try_parse_left_expr(p, x_expr);
    if (left == -1) return 0; //no left expr, probably not an expression

    x_expr->span.end = p->x.s.end;
    x_expr->op = op_none;

    //parse ops
    while(1) {
        op x_op;
        unsigned x_op_prec;

        switch (parse_peek(p)->tt) {
            case t_rparen: {
                if (l_paren) {
                    //consume paren
                    parse_next(p);
                    l_paren = NULL;
                    x_expr->span.end = p->x.s.end; //update span end

                    //try to parse expr, in case this is a cast
                    parser peek_parser = *p;

                    expr cast_expr;
                    if (try_parse_expr(&peek_parser, &cast_expr, op_prec)) {
                        x_op = op_cast;
                        x_op_prec = 4;
                        break;
                    }

                    //restart loop
                    continue;
                } else {
                    return 1;
                }
            }

            case t_set: {
                x_op = op_set;
                x_op_prec = 0;
                break;
            }

            case t_add: {
                x_op = op_add;
                x_op_prec = 2;
                break;
            }

            case t_sub: {
                x_op = op_sub;
                x_op_prec = 2;
                break;
            }

            case t_div: {
                x_op = op_div;
                x_op_prec = 3;
                break;
            }

            case t_mul: {
                x_op = op_mul;
                x_op_prec = 3;
                break;
            }

            default: return 1;
        }

        //dont parse any more
        if (x_op_prec < op_prec) break;
        else parse_next(p); //otherwise increment parser

        //op already exists, detach
        //happens every time after first loop since each loop sets op
        if (x_expr->op != op_none) {
            parse_detach(x_expr);
        }

        expr* r_expr;

        //allocate or alias, depending on whether left side exists
        if (left) r_expr = malloc(sizeof(expr));
        else r_expr = x_expr;

        // check for assignment, ex. +=, /=, and not already an assignment op (op_set)
        if (x_op!=op_set && parse_next_eq(p, t_set))
            r_expr->assign=1;

        if (!try_parse_expr(p, r_expr, x_op_prec)) {
            if (left) free(r_expr);
            return 1;
        }

        if (left) {
            x_expr->op = x_op;
            x_expr->right=r_expr;
        }

        left=1; //left now exists whether it did or not
    }

    return 1;
}

void print_expr(expr* e) {
    if (e->flags & left_neg) {
        printf("-");
    }
    if (e->flags & left_ref) printf("&");

    switch (e->left) {
        case left_num: print_num((num*)e->x); break;
        case left_char: printf("%c", *(char*)e->x); break;
        case left_str: printf("\"%s\"", (char*)e->x); break;
        case left_expr: {
            printf("("); print_expr((expr*)e->x); printf(")");
            break;
        }
        case left_access: printf("%s", (char*)e->x); break;
        case left_call: {
            fn_stmt* fn = ((fn_call*)e->x)->target; //TODO: make my language fix this
            if (!fn) return;

            printf("%s(%lu args)", fn->name, ((fn_call*)e->x)->args.length);
            break;
        }
    }

    switch (e->op) {
        case op_cast: printf(" <: ("); print_expr(e->right); printf(")"); break;
        case op_add: printf(" + ("); print_expr(e->right); printf(")"); break;
        case op_div: printf(" / ("); print_expr(e->right); printf(")"); break;
        case op_mul: printf(" * ("); print_expr(e->right); printf(")"); break;
        case op_sub: printf(" - ("); print_expr(e->right); printf(")"); break;
        case op_set: printf(" = ("); print_expr(e->right); printf(")"); break;
        case op_idx: printf("["); print_expr(e->right); printf("]"); break;
        default: return;
    }
}

void print_block(block* b) {
    vector_iterator x = vector_iterate(&b->stmts);
    while (vector_next(&x)) {
        stmt* s = x.x;
        switch (s->t) {
            case s_ret: printf("return ");
            case s_expr: print_expr(s->x); printf(";"); break;

            case s_bind: {
                bind_stmt* bind = s->x;
                printf("%s %s = ", type_str(&bind->ty.td), bind->name);
                if (bind->uninitialized) printf("uninitialized"); else print_block(&bind->val);
                break;
            }

            case s_ret_block: printf("return ");
            case s_block: print_block(s->x); break;

            case s_fn: {
                fn_stmt* fn = s->x;
                printf("fn %s(%lu) -> %s {\n", fn->name, fn->args.length, type_str(&fn->ty.td));
                print_block(&fn->val);
                printf("}");
                break;
            }

            case s_fn_arg: {
                printf("argument"); break;
            }

            default:;
        }

        printf("\n");
    }
}

void expr_free(expr* e) {
    switch (e->left) {
        case left_expr: expr_free(e->x); break;
        case left_call: free(e->x);
        default:;
    }

    if (e->op != op_none) expr_free(e->right);

    free(e);
}

void stmt_free(stmt* s) {
    switch (s->t) {
        case s_fn: fn_free(s->x); break;
        case s_fn_arg: break;

        case s_ret_block: case s_block: block_free(s->x); break;
        case s_ret: case s_expr: expr_free(s->x); break;

        default: free(s->x);
    }
}