#include "frontend.c"

typedef struct {
    frontend* fe;
    token x;

    unsigned long pos;

    block* current;
} parser;

int parse_eof(parser* p) {
    return p->pos == p->fe->tokens.length;
}

int parse_next(parser* p) {
    token* x = vector_get(&p->fe->tokens, p->pos);
    if(x) p->x = *x;
    p->pos++;
    return p->pos <= p->fe->tokens.length;
}

token* parse_peek(parser* p, int i) {
    if (p->pos+i-1 >= p->fe->tokens.length) return NULL;
    else return vector_get(&p->fe->tokens, p->pos+i-1);
}

int parse_next_eq(parser* p, token_type tt) {
    token* x = parse_peek(p, 1);
    if (x && x->tt == tt) return parse_next(p);
    else return 0;
}

//for debugging convenience
int separator(parser* p) {
    token* t = parse_peek(p, 1);
    return t==NULL || t->tt==t_sep || t->tt==t_rbrace || t->tt==t_rparen || t->tt==t_ridx;
}

typedef struct {
    span s;
    char* typename;
    typedata td;
} type_id;

typedef enum {
    op_none,
    op_sub, op_add, op_mul, op_div, op_idx,
    op_set
} op;

typedef enum {
    left_access = 0x1,
    left_call = 0x2,
    left_num = 0x4,
    left_expr = 0x8,
    //prefix ops
    left_neg = 0x10,
    left_ref = 0x20,
    //prefix incr/decr: ++x
    left_incr = 0x40,
    left_decr = 0x80,
    //affix ops: x++
    left_incr_after = 0x100,
    left_decr_after = 0x200
} left;

typedef struct {
    type_id ty;
    char* name;
    char uninitialized;
    block val;
} bind_stmt;

typedef enum {
    fn_arg_restrict = 0x01
} fn_arg_flags;

typedef struct {
    type_id ty;
    char* name;
    fn_arg_flags flags;
} fn_arg;

typedef struct {
    type_id ty;
    vector args;
    char* name;
    block val;
} fn_stmt;

typedef struct {
    fn_stmt* target;
    vector args;
} fn_call;

typedef struct {
    typedata* ty;
    fn_call* call;
} fn_validate;

/// linked list
typedef struct {
    op op;
    /// assign after op
    char assign;

    left left;
    span left_span;
    void* left_ref;
    typedata left_ty;
    //always expr
    void* right;
} expr;

int parse_expr(parser* p, expr* x_expr, unsigned op_prec); //forward decl

int generalize(typedata* x) {
    switch (x->prim) {
        case t_int:
        case t_i8: case t_i16: case t_i32: case t_i64:
            return t_int;

        case t_uint:
        case t_u8: case t_u16: case t_u32: case t_u64:
            return t_uint;

        default:
            if (x->flags & ty_ptr) return t_void; //void* can be implicitly casted to pointers of any type
            else return x->prim;
    }
}

/// compares from and to with the premise that to is the expected type
/// will mutate from and to the more specific type
int type_eq(typedata* from, typedata* to) {
    //TODO: overflow checking
    if (from->prim == generalize(to) || from->prim == t_any) {
        from->prim = to->prim;
    } else if (to->prim == generalize(from) || to->prim == t_any) {
        to->prim = from->prim;
    }

    //both are arrays but sizes do not match
    if (from->flags & ty_arr && to->flags & ty_arr && from->size != to->size)
        return 0;

    return (from->prim == to->prim || to->prim == t_any)
           && (from->flags == to->flags //if either flags==flags or flags==flags without const
               || from->flags == (to->flags & ~ty_const)); //TODO: compare compound types and functions
}

/// returns 1 if thrown
int type_eq_throw(parser* p, span* s, typedata* from, typedata* to, char* err) {
    if (!type_eq(from, to)) {
        throw(p->fe, s, isprintf(err, type_str(from), type_str(to)));
        return 1;
    } else {
        return 0;
    }
}

expr* parse_idx(parser* p) {
    if (parse_next_eq(p, t_lidx)) {
        span* l_idx = &p->x.s; //record starting bracket

        expr* i_expr = malloc(sizeof(expr));

        if (!parse_expr(p, i_expr, 0)) {
            free(i_expr);
            throw(p->fe, &p->x.s, "expected expression as index"); return NULL;
        }

        if (!parse_next_eq(p, t_ridx)) {
            throw(p->fe, &p->x.s, "expected right bracket to end index");
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
            if (n->ty != num_integer) return throw(p->fe, &p->x.s, "expected integer");
            *x = num_to_long(n);
        }

        if (!parse_next_eq(p, t_ridx)) {
            throw(p->fe, &p->x.s, "expected right bracket to end length");
            note(p->fe, l_idx, "other bracket here");
        }

        return 1;
    } else {
        return 0;
    }
}

/// searches in global and current block
stmt* find_declaration(parser* p, char* target) {
    void* x = map_find(&p->fe->global.declarations, &target);
    if (x) return x;

    void* x2 = map_find(&p->current->declarations, &target);
    if (x2) return x2;

    return NULL;
}

/// parse anything that can be a type attribution, to be resolved later
int parse_type_id(parser* p, type_id* tid) {
    if (!parse_next_eq(p, t_id)) return 0;

    tid->s.start = p->x.s.start;
    tid->td.flags = 0;

    if (strcmp(p->x.val, "const")==0) {
        tid->td.flags |= ty_const;

        if (!parse_next_eq(p, t_id)) return 0;
    }

    tid->typename = p->x.val;

    if (parse_length(p, &tid->td.size)) //try parse array size
        tid->td.flags |= ty_arr;

    if (parse_next_eq(p, t_mul))
        tid->td.flags |= ty_ptr;

    tid->s.end = p->x.s.end;

    return 1;
}

/// resolve a parsed type id
void resolve_type_id(parser* p, type_id* tid) {
    tid->td.prim = prim_from_str(tid->typename);
    //TODO: resolved compound types
}

void parse_set_left(expr* expr, left l, void* x) {
    expr->left |= l;
    expr->left_ref = x;
}

void parse_detach(expr* x_expr) {
    expr* detached = malloc(sizeof(expr));
    memcpy(detached, x_expr, sizeof(expr));

    x_expr->left = left_expr;
    x_expr->left_ref = detached;
}

void stmt_free(stmt* s);

void block_init(block* b) {
    b->stmts = vector_new(sizeof(stmt));
    b->ret_type.prim = t_any; b->ret_type.flags = 0;

    b->unresolved = map_new();
    map_configure_string_key(&b->unresolved, sizeof(stmt*));
}

void block_subinit(parser* p, block* b) {
    block_init(b);
    //copy scope if not global
    if (p->current != &p->fe->global) {
        map_copy(&p->current->declarations, &b->declarations);
    } else {
        b->declarations = map_new();
        map_configure_string_key(&b->declarations, sizeof(stmt));
    }
}

void block_subend(block* superblock, block* b) {
    // copy unresolved stuff to superblock
    map_iterator iter = map_iterate(&b->unresolved);
    while (map_next(&iter)) {
        map_insertcpy(&superblock->unresolved, iter.key, iter.x);
    }
}

// pretty much the same except we throw errors here
void block_end(parser* p, block* b) {
    map_iterator iter = map_iterate(&b->declarations);
    while (map_next(&iter)) {
        stmt* s = iter.x;
        if (s->t == s_unresolved_type) {
            throw(p->fe, &s->s, "unresolved type");
        } else if (s->t == s_unresolved_fn) {
            throw(p->fe, &s->s, "unresolved function");
        }
    }
}

void block_free(block* b) {
    vector_iterator iter = vector_iterate(&b->stmts);
    while (vector_next(&iter)) {
        stmt_free(iter.x);
    }

    vector_free(&b->stmts);
    map_free(&b->declarations);
}

int parse_block_braced(parser* p, block* b); //forward decl
int parse_block(parser* p, block* b); //forward decl

int parse_separated(parser* p, token_type sep, token_type end) {
    if (parse_next_eq(p, end)) return 1;

    if (!parse_next_eq(p, sep)) {
        throw(p->fe, &p->x.s, "expected separator");
    }

    if (parse_next_eq(p, end)) return 1; //end can come before or after arguments

    return 0;
}

int parse_stmt_end(parser* p) {
    //any separator works, but only semicolon is parsed
    if (!separator(p)) throw(p->fe, &p->x.s, "expected semicolon after statement");
    return parse_next_eq(p, t_sep);
}

void validate_fn_call(parser* p, stmt* unresolved, stmt* candidate) {
    switch (unresolved->t) {
        case s_unresolved_fn: {
            if (candidate->t != s_fn) {
                throw(p->fe, &unresolved->s, "name does not reference a function");
                note(p->fe, &candidate->s, "defined here");
                return;
            }

            fn_stmt* fn = candidate->x;

            fn_validate* fnv = unresolved->x;
            fnv->call->target = fn;

            type_eq_throw(p, &unresolved->s, &fn->ty.td, fnv->ty, "function result of type %s is not an %s");

            vector_iterator iter = vector_iterate(&fn->args);
            vector_iterator iter2 = vector_iterate(&fnv->call->args);

            while (vector_next(&iter)) {
                if (!vector_next(&iter2)) {
                    throw(p->fe, &unresolved->s,
                          isprintf("too little arguments: expected %lu, got %lu", fn->args.length, fnv->call->args.length));
                    break;
                }

                expr* arg_expr = iter2.x;
                type_id* arg_ty = &((fn_arg*) iter2.x)->ty;
                if (type_eq_throw(p, &arg_expr->left_span, &arg_expr->left_ty, &arg_ty->td, "expected %s for function argument, got %s")) {
                    note(p->fe, &arg_ty->s, "as specified here");
                }
            }

            if (vector_next(&iter2)) {
                throw(p->fe, &unresolved->s, isprintf("%lu extra arguments provided", fnv->call->args.length - fn->args.length));
            }
        }

        case s_unresolved_type:
        default:;
    }
}

void fn_free(fn_stmt* fn) {
    vector_free(&fn->args); block_free(&fn->val); free(fn);
}

void print_block(block* b);

/// parses bind or set, returns name of binding
int parse_bind_fn(parser* p, stmt** new_stmt) {
    type_id tid;

    if (parse_type_id(p, &tid) && parse_next_eq(p, t_id)) {
        stmt s;

        char* target;

        resolve_type_id(p, &tid);

        s.s.start = p->x.s.start;
        target = p->x.val; //name of binding

        if (parse_next_eq(p, t_lparen)) {
            //parse fn
            fn_stmt* fn = malloc(sizeof(fn_stmt));
            span fn_paren = p->x.s;

            fn->ty = tid;
            fn->name = target;
            fn->args = vector_new(sizeof(fn_arg));
            block_subinit(p, &fn->val);

            if (!parse_next_eq(p, t_rparen)) {
                //parse arguments
                while (1) {
                    fn_arg* arg = vector_push(&fn->args);

                    if (!parse_type_id(p, &arg->ty)) throw(p->fe, &p->x.s, "expected argument type");
                    resolve_type_id(p, &tid);

                    if (!parse_next_eq(p, t_id)) throw(p->fe, &p->x.s, "expected argument name");

                    arg->name = p->x.val;

                    stmt* fn_arg_s = vector_push(&fn->val.stmts);
                    fn_arg_s->t = s_fn_arg; fn_arg_s->s=p->x.s; fn_arg_s->x = arg;

                    map_insertcpy(&fn->val.declarations, &arg->name, fn_arg_s);

                    if (parse_separated(p, t_comma, t_rparen)) break;

                    if (separator(p)) {
                        fn_free(fn);

                        throw(p->fe, &p->x.s, "expected matching paren for function declaration");
                        note(p->fe, &fn_paren, "other paren here");
                        return 1; //consume
                    }
                }
            }

            s.s.end = p->x.s.end; //span only includes function header

            stmt* x = find_declaration(p, target);
            if (x) {
                throw(p->fe, &s.s, "name already used");
                note(p->fe, &x->s, "declaration here");
                fn_free(fn);
                return 1;
            }

            fn->val.ret_type = fn->ty.td; //set expected return type
            if (!parse_block(p, &fn->val)) throw(p->fe, &p->x.s, "expected block for function value");

            s.t=s_fn;
            s.x=fn;

            //validate unresolved calls to new function
            vector* unresolved = map_find(&p->current->unresolved, &target);
            if (unresolved) {
                vector_iterator iter = vector_iterate(unresolved);
                while (vector_next(&iter)) validate_fn_call(p, iter.x, &s);
            }
        } else {
            stmt* x = find_declaration(p, target);
            if (x) { //statement is resolved
                throw(p->fe, &p->x.s, "name already used");
                note(p->fe, &x->s, "declaration here");
                return 1;
            }

            bind_stmt* bind = malloc(sizeof(bind_stmt));
            bind->name = target;
            bind->ty = tid;

            if (parse_next_eq(p, t_set)) {
                block_subinit(p, &bind->val);

                if (!parse_block(p, &bind->val)) {
                    throw(p->fe, &p->x.s, "expected block/expression in binding");
                    bind->uninitialized=1;
                } else {
                    type_eq_throw(p, &p->x.s, &bind->val.ret_type, &bind->ty.td, "%s cannot be bound to a %s");
                }
            } else {
                bind->uninitialized=1;
                parse_stmt_end(p);
            }

            s.t=s_bind;
            s.x=bind;
            s.s.end = p->x.s.end;
        }

        *new_stmt = vector_pushcpy(&p->current->stmts, &s);
        map_insertcpy(&p->current->declarations, &target, &s);

        return 1;
    } else {
        return 0;
    }
}

stmt* parse_stmt(parser* p) {
    parser peek_parser = *p;

    stmt* s;
    if (parse_bind_fn(&peek_parser, &s)) {
        *p = peek_parser; //update pos
        return s;
    }

    int ret = parse_next_eq(p, t_return);
    stmt new_s;

    expr* e = malloc(sizeof(expr));
    if (parse_expr(p, e, 0)) {
        if (!parse_stmt_end(p)) ret=1; //if there isn't a semicolon, it shall be qualified as a return statement, which can later be validated

        new_s.t = ret ? s_ret : s_expr;
        new_s.x = e;

        new_s.s.start=e->left_span.start;
        new_s.s.end=p->x.s.end;

        if (ret) type_eq_throw(p, &e->left_span, &e->left_ty, &p->current->ret_type, "%s does not match return type %s");

        return vector_pushcpy(&p->current->stmts, &new_s);
    } else {
        free(e);
    }

    new_s.s.start = p->x.s.end;
    block* sub_b = malloc(sizeof(block)); //try parsing block
    block_subinit(p, sub_b);

    if (parse_block_braced(p, sub_b)) {
        new_s.t = ret ? s_ret_block : s_block;
        new_s.x = sub_b;

        new_s.s.end = p->x.s.end;

        if (ret) type_eq_throw(p, &e->left_span, &sub_b->ret_type, &p->current->ret_type, "%s does not match block return type %s");

        return vector_pushcpy(&p->current->stmts, &new_s);
    }

    free(s); return NULL;
}

int parse_block_inner(parser* p, block* b) {
    if (parse_next_eq(p, t_lbrace)) {
        span l_brace = p->x.s;

        char returned=0;
        span ret_span;

        while (!parse_next_eq(p, t_rbrace)) {
            if (separator(p)) {
                throw(p->fe, &p->x.s, "expected matching brace for block");
                note(p->fe, &l_brace, "other brace here");
                return 0;
            }

            stmt* s = parse_stmt(p);
            if (!s) {
                throw(p->fe, &p->x.s, "expected statement");
            } else if (s->t == s_ret || s->t == s_ret_block) { //return stmt
                if (returned) {
                    warn(p->fe, &s->s, "block has already returned by this point");
                    note(p->fe, &ret_span, "returned here");
                } else {
                    ret_span = s->s;
                    returned=1;
                }
            }
        }

        if (b->ret_type.prim==t_any) {
            b->ret_type.prim = t_void;
        }

        //if there are flags / return type isnt void, then we check if function has been returned
        if (b->ret_type.flags || b->ret_type.prim!=t_void) {
            if (!returned) {
                stmt* last_stmt = vector_get(&b->stmts, b->stmts.length - 1);
                throw(p->fe, &last_stmt->s, "block does not return, even though block has a return type");
            }
        }

        return 1;
    } else {
        return 0;
    }
}

// slight code duplication

/// parse a braced block, separator or one-statement blocks are disallowed
int parse_block_braced(parser* p, block* b) {
    block* old_b = p->current;
    p->current = b;

    int ret = parse_block_inner(p, b);
    block_subend(old_b, b);
    p->current = old_b;
    return ret;
}

int parse_block(parser* p, block* b) {
    block* old_b = p->current;
    p->current = b;

    //neither block nor separator nor one statement block
    if (!(parse_block_inner(p, b) || parse_next_eq(p, t_sep) || parse_stmt(p))) {
        throw(p->fe, &p->x.s, "expected block/statement");

        p->current = old_b;
        return 0;
    }

    block_subend(old_b, b);
    p->current = old_b;
    return 1;
}

void parse(frontend* fe) {
    parser p = {fe, .pos=0};

    block_init(&p.fe->global);
    p.fe->global.declarations = map_new();
    map_configure_string_key(&p.fe->global.declarations, sizeof(stmt));

    p.current = &p.fe->global;

    while (!parse_eof(&p) && parse_stmt(&p));

    block_end(&p, &p.fe->global);
}

void resolve(parser* p, char* target, span s, stmt_t t, void* validate, size_t validate_size) {
    stmt unresolved = {t, s, .x=validate};

    stmt* candidate = find_declaration(p, target);
    if (candidate) {
        validate_fn_call(p, &unresolved, candidate);
    } else {
        //allocate validator
        unresolved.x = malloc(validate_size);
        memcpy(unresolved.x, validate, validate_size);

        vector* x = map_find(&p->current->unresolved, &target);

        if (!x) {
            //initialize new vector
            x = map_insert(&p->current->unresolved, &target).val;
            *x = vector_new(sizeof(stmt));
        }

        vector_pushcpy(x, &unresolved);
    }
}

int parse_left_expr(parser* p, expr* x_expr) {
    x_expr->left=0;
    x_expr->left_ty.flags=0;

    if (parse_next_eq(p, t_ref)) {
        x_expr->left |= left_ref;
        x_expr->left_ty.flags |= ty_ptr;
    }

    if (parse_next_eq(p, t_sub)) x_expr->left |= left_neg;
    if (parse_next_eq(p, t_incr)) x_expr->left |= left_incr;
    if (parse_next_eq(p, t_decr)) x_expr->left |= left_decr;

    if (parse_next_eq(p, t_num)) {
        //copy to left
        num* x = p->x.val;
        //set primitive
        switch (x->ty) {
            case num_decimal: x_expr->left_ty.prim=t_float; break;
            case num_unsigned: x_expr->left_ty.prim=t_uint; break;
            case num_integer: x_expr->left_ty.prim=t_int; break;
        }

        x_expr->left_ty.data = x;
        x_expr->left_span = p->x.s;
        parse_set_left(x_expr, left_num, x);
    } else if (parse_next_eq(p, t_id)) {
        char* target = p->x.val;
        x_expr->left_span = p->x.s;

        if (parse_next_eq(p, t_lparen)) {
            span fn_paren = p->x.s;

            fn_call* fc = malloc(sizeof(fn_call));
            fc->args = vector_new(sizeof(expr));

            //parse arguments
            while (!parse_next_eq(p, t_rparen)) {
                if (separator(p)) {
                    free(fc); vector_free(&fc->args);

                    throw(p->fe, &p->x.s, "expected matching paren for function call");
                    note(p->fe, &fn_paren, "other paren here");
                    return 0;
                }

                expr* expr_arg = vector_push(&fc->args);
                if (!parse_expr(p, expr_arg, 0)) {
                    free(fc); vector_free(&fc->args);
                    return throw(p->fe, &p->x.s, "expected expression");
                }

                if (parse_next_eq(p, t_rparen)) break;

                if (!parse_next_eq(p, t_comma)) {
                    throw(p->fe, &p->x.s, "expected comma to separate arguments");
                }

                if (parse_next_eq(p, t_rparen)) break; //paren can come before or after arguments
            }

            x_expr->left_ty.prim = t_any;
            fc->target = NULL;

            fn_validate validate = {.ty=&x_expr->left_ty, .call=fc};
            resolve(p, target, x_expr->left_span, s_unresolved_fn, &validate, sizeof(fn_validate));

            x_expr->left_span.end = p->x.s.end;
            parse_set_left(x_expr, left_call, fc);
        } else {
            expr* r_expr = parse_idx(p);

            stmt* x = find_declaration(p, target);
            if (!x) {
                throw(p->fe, &x_expr->left_span, "variable not found in scope");
            } else if (x->t != s_bind && x->t != s_fn_arg) {
                throw(p->fe, &x_expr->left_span, "name is not a variable");
            } else {
                bind_stmt* bind = x->x;
                x_expr->left_ty = bind->ty.td;

                if (r_expr && !(bind->ty.td.flags & ty_arr || bind->ty.td.flags & ty_ptr)) {
                    throw(p->fe, &x_expr->left_span, "cannot index, not an array");
                }
            }

            x_expr->left_span.end = p->x.s.end;

            if (r_expr) {
                //set left to index expr
                expr* index_expr = malloc(sizeof(expr));
                parse_set_left(index_expr, left_access, target);
                index_expr->right=r_expr;

                parse_set_left(x_expr, left_expr, index_expr);
            } else {
                //not call or index, simple access
                parse_set_left(x_expr, left_access, target);
            }
        }
    } else {
        return -1; //no expression at all
    }

    if (parse_next_eq(p, t_incr)) x_expr->left |= left_incr_after;
    if (parse_next_eq(p, t_decr)) x_expr->left |= left_decr_after;

    // quick check to find if we are negating an unsigned value
    if (generalize(&x_expr->left_ty) == t_uint && x_expr->left & left_neg) {
        throw(p->fe, &x_expr->left_span, "cannot negate an unsigned value");
    }

    return 1;
}

int parse_expr(parser* p, expr* x_expr, unsigned op_prec) {
    int left=0; //error in the left side: return the right side instead
    span* l_paren=NULL;

    if (parse_next_eq(p, t_lparen)) {
        //parentheses around expression, set base precedence to zero
        l_paren = &p->x.s;
        op_prec = 0;
    }

    //parse left side and set left status
    left = parse_left_expr(p, x_expr);
    if (left == -1) return 0; //no left expr, probably not an expression

    x_expr->op = op_none;

    //parse ops
    while(1) {
        op x_op;

        unsigned x_op_prec;

        token* tok = parse_peek(p, 1);
        if (!tok) return 1; //no more tokens

        switch (tok->tt) {
            case t_rparen: {
                if (l_paren) {
                    //consume paren
                    parse_next(p);
                    l_paren = NULL;
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

        if (!parse_expr(p, r_expr, x_op_prec)) {
            if (left) free(r_expr);
            throw(p->fe, &p->x.s, "expected expression after operator");
            return 1;
        }

        //check that types match
        type_eq_throw(p, &x_expr->left_span, &r_expr->left_ty, &x_expr->left_ty, "%s is not a %s");

        if (left) {
            x_expr->op = x_op;
            x_expr->right=r_expr;
        }

        left=1; //left now exists whether it did or not
    }

    return 1;
}

void print_expr(expr* e) {
    if (e->left & left_neg) {
        printf("-");
    }
    if (e->left & left_ref) printf("&");

    if (e->left & left_num) print_num((num*)e->left_ref);
    else if (e->left & left_expr) {
        printf("("); print_expr((expr*)e->left_ref); printf(")");
    }
    else if (e->left & left_access) printf("%s", (char*)e->left_ref);
    else if (e->left & left_call) {
        fn_stmt* fn = ((fn_call*)e->left_ref)->target; //TODO: make my language fix this
        if (!fn) return;

        printf("%s(%lu args)", fn->name, ((fn_call*)e->left_ref)->args.length);
    }

    switch (e->op) {
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

            case s_unresolved_fn: {
                printf("unresolved fn"); break;
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
    if (e->left & left_expr) expr_free(e->left_ref);
    else if (e->left & left_call) free(e->left_ref);

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