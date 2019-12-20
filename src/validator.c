#include "parser.c"

/// only generalizes primitives
prim_type generalize(prim_type x) {
    switch (x) {
        case ty_int:
        case ty_i8: case ty_i16: case ty_i32: case ty_i64:
            return ty_int;

        case ty_uint:
        case ty_u8: case ty_u16: case ty_u32: case ty_u64:
            return ty_uint;

        default: return x;
    }
}

/// uses flags to get more specific generalization
int generalize_ptr(typedata* x) {
    if (x->flags & ty_ptr) return t_ptr;
    else return generalize(x->prim);
}

/// compares from and to with the premise that to is the expected type
/// will mutate from and to the more specific type
int type_eq(typedata* from, typedata* to) {
    //TODO: overflow checking
    //constraint primitives first
    if (from->prim == generalize(to->prim) || from->prim == ty_any) {
        from->prim = to->prim;
    } else if (to->prim == generalize(from->prim) || to->prim == ty_any) {
        to->prim = from->prim;
    }

    //both are arrays but sizes do not match
    if (from->flags & ty_arr && to->flags & ty_arr && from->size != to->size)
        return 0;

    //compare flags
    return (from->prim == to->prim || to->prim == ty_any)
           && (from->flags == to->flags
               || from->flags == (to->flags & ~ty_const) //if either flags==flags or flags==flags without const
                || (((from->flags ^ to->flags) == ty_const) && ((from->flags | to->flags) & ty_ptr)==0)); //if flags without const==flags without const && neither are ptrs

    //TODO: compare compound types and functions
}

/// returns 1 if thrown
int type_eq_throw(frontend* fe, span* s, typedata* from, typedata* to, char* err) {
    if (!type_eq(from, to)) {
        throw(isprintf(err, type_str(from), type_str(to)));
        return 1;
    } else {
        return 0;
    }
}

typedef struct svalidator {
    frontend* fe;
    struct svalidator* super;
    module* b;
    map locals;
} validator;

/// searches hierarchy for declaration
stmt* find_declaration(validator* v, char* target) {
    void* x = map_find(&v->b->declarations, &target);
    if (x) return x;

    void* local = map_find(&v->locals, &target);
    if (local) return local;

    if (v->super) {
        return find_declaration(v->super, target);
    } else {
        return NULL;
    }
}

void typeid_type(validator* v, id_type* tid) {
    if (!tid->resolved) {
        tid->td.prim = prim_from_str(tid->name);
        //TODO: compound types and stuff
        tid->resolved=1;
    }
}

void expr_type(validator* v, expr* e, typedata* td);
void module_type(validator* super, span* s, module* b, id_type* tid);

/// get type of declaration
/// non-recursive, memoized
int declaration_type(validator* v, stmt* s, typedata* td) {
    //TODO: metatypes and meta-functions
    switch (s->t) {
        case s_fn: {
            fn_stmt* fn = s->x;
            typeid_type(v, &fn->ty);
            *td = fn->ty.td;
            break;
        }

        case s_bind: {
            bind_stmt* bind = s->x;
            typeid_type(v, &bind->ty);
            *td = bind->ty.td;
            break;
        }

        default: return 0;
    }

    return 1;
}

/// also typechecks blocks in statements
int stmt_type(validator* v, stmt* s, typedata* td, id_type* ret) {
   switch (s->t) {
        case s_ret: {
            expr_type(v, s->x, td);
            type_eq_throw(v->fe, &s->s, td, &ret->td, "incorrect return type %s for %s");

            break;
        }
        case s_block: //anonymous blocks return from function
        case s_ret_block: module_type(v, &s->s, s->x, ret); break;
        case s_expr: expr_type(v, s->x, td); break;
        case s_fn: {
            fn_stmt* fn = s->x;
            typeid_type(v, &fn->ty);

            vector_iterator iter = vector_iterate(&fn->args);
            while (vector_next(&iter)) {
                typeid_type(v, iter.x);
            }

            if (!fn->extern_fn) module_type(v, &s->s, &fn->val, &fn->ty);
            *td = fn->ty.td;

            break;
        }
        case s_bind: {
            bind_stmt* bind = s->x;
            typeid_type(v, &bind->ty);

            module_type(v, &s->s, &bind->val, &bind->ty);
            *td = bind->ty.td;

            scope_insert(v->fe, &v->locals, bind->name, s);

            break;
        }

        default: return 0;
    }

    return 1;
}

int expr_left(validator* v, expr* e, typedata* td) {
    if (e->flags & left_ref) {
        e->flags ^= left_ref;
        //parse anything without reference
        int x = expr_left(v, e, td);
        //reapply reference
        e->flags ^= left_ref;
        td->flags |= ty_ptr;

        return x;
    }

    switch (e->left) {
        case left_expr: {
            expr_type(v, e->x, td);
            break;
        }
        case left_num: {
            //set primitive
            switch (((num*)e->x)->ty) {
                case num_decimal: td->prim=ty_float; break;
                case num_unsigned: td->prim=ty_uint; break;
                case num_integer: td->prim=ty_int; break;
            }

            td->flags |= ty_const;
            //reference num for size checks
            td->data = e->x;
            break;
        }
        case left_char: {
            td->prim = ty_char;
            td->flags |= ty_const;
            break;
        }
        case left_str: {
            td->prim = ty_str;
            td->flags |= ty_const;

            td->data = e->x;
            break;
        }
        case left_access: {
            id_type* tid = e->x;

            stmt* decl = find_declaration(v, tid->name);
            if (!decl) return throw("cannot find name in scope");

            if (tid->td.flags) { // transfer flags and set metatype
                if (td->prim != ty_meta) {
                    throw("name does not reference a type");
                    note(&decl->s, "name defined here");

                    return 0;
                }

                td->flags |= tid->td.flags; //fixme: merge flags, idk if this works
            }

            declaration_type(v, decl, td);
            break;
        }
        case left_call: {
            fn_call* fc = e->x;
            stmt* decl = find_declaration(v, fc->name);

            if (!decl) return throw("cannot find function");

            declaration_type(v, decl, td);

            if (decl->t != s_fn) {
                throw("does not reference function");
                note(&decl->s, "declared here");
                return 0;
            }

            fn_stmt* fn = decl->x;

            fc->target = fn;

            vector_iterator iter = vector_iterate(&fn->args);
            vector_iterator iter2 = vector_iterate(&fc->args);

            while (vector_next(&iter)) {
                if (!vector_next(&iter2)) {
                    throw(
                          isprintf("not enough arguments: expected %lu, got %lu", fn->args.length, fc->args.length));
                    break;
                }

                expr* arg_expr = iter2.x;
                id_type* arg_ty = &((fn_arg*) iter.x)->ty;
                typeid_type(v, arg_ty); //resolve type if unresolved

                typedata arg_expr_type;
                expr_type(v, arg_expr, &arg_expr_type);

                if (type_eq_throw(v->fe, &arg_expr->span, &arg_expr_type, &arg_ty->td,
                                  "function argument of type %s is not an %s")) {
                    note(&arg_ty->s, "as specified here");
                }
            }

            if (vector_next(&iter2)) {
                throw(isprintf("%lu extra argument(s) provided", fc->args.length - fn->args.length));
            }

            break;
        }
    }

    prim_type generalized = generalize_ptr(td);
    int num_type = generalized == ty_uint || generalized == ty_int || generalized == ty_float || generalized == t_ptr;

    //quick "type" checks
    if (e->flags & left_num_op && !num_type) {
        throw(isprintf("cannot increment/decrement a %s", type_str(td)));
    }

    if (generalize_ptr(td) == ty_uint && e->flags & left_neg) {
        throw("cannot negate an unsigned value");
    }

    return 1;
}

void expr_type(validator* v, expr* e, typedata* td) {
    td->prim = ty_any;
    td->flags = 0;

    expr_left(v, e, td);

    if (e->op != op_none) {
        expr* right = e->right;

        typedata right_ty;
        expr_type(v, right, &right_ty);

        type_eq_throw(v->fe, &right->span, &right_ty, td, "cannot operate %s on an %s");
    }
}

validator validator_new(validator* super, module* b, frontend* fe) {
    validator v = {.b=b, .fe=fe, .locals=map_new(), .super=super};
    map_configure_string_key(&v.locals, sizeof(stmt));

    return v;
}

void module_type(validator* super, span* s, module* b, id_type* tid) {
    validator v = validator_new(super, b, super->fe);

    char returned=0;
    span ret_span;

    vector_iterator iter = vector_iterate(&b->stmts);
    while (vector_next(&iter)) {
        stmt* x = iter.x;

        typedata x_type;
        if (!stmt_type(&v, x, &x_type, tid)) continue;

        if (x->t == s_ret || x->t == s_ret_block) {
            if (returned) {
                warn("module has already returned by this point");
                note(&ret_span, "returned here");
            } else {
                ret_span = x->s;
                returned = 1;
            }
        }
    }

    if (tid->td.prim == ty_any) {
        tid->td.prim = ty_void;
    }

    //if there are flags / return type isnt void, then we check if function has been returned
    if (tid->td.flags || tid->td.prim != ty_void) {
        if (!returned) {
            throw("module does not return, even though module has a return type");
        }
    }
}

void global_type(validator* super, module* b, frontend* fe) {
    validator v = validator_new(super, b, fe);

    vector_iterator iter = vector_iterate(&v.b->stmts);
    while (vector_next(&iter)) {
        stmt* s = iter.x;
        typedata td;

        switch (s->t) {
            case s_ret_block:
            case s_ret: throw("return statements are not allowed in global scope"); break;
            case s_block:  global_type(&v, s->x, v.fe);
            case s_expr: expr_type(&v, s->x, &td); break;

            case s_fn: {
                fn_stmt* fn = s->x;
                typeid_type(&v, &fn->ty);
                if (!fn->extern_fn) module_type(&v, &s->s, &fn->val, &fn->ty);

                break;
            }

            case s_bind: {
                bind_stmt* bind = s->x;
                typeid_type(&v, &bind->ty);
                module_type(&v, &s->s, &bind->val, &bind->ty);

                scope_insert(fe, &v.locals, bind->name, s);

                break;
            }

            default: ;
        }
    }
}

void validate(frontend* fe) {
    global_type(NULL, &fe->global, fe);
}