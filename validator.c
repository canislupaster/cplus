#include "frontend.c"

typedef struct {
    frontend* fe;
    map scope;
    vector new_scope;
} validator;

typedef enum {
    t_int, t_uint, t_float,

    t_i8, t_i16, t_i32, t_i64,
    t_u8, t_u16, t_u32, t_u64,
    t_f8, t_f16, t_f32, t_f64,

    t_bool, t_str, t_char,
    t_void, t_func, t_compound
} prim_type;

int generalize_prim(prim_type x) {
    switch (x) {
        case t_int:
        case t_i8: case t_i16: case t_i32: case t_i64:
            return t_int;

        case t_uint:
        case t_u8: case t_u16: case t_u32: case t_u64:
            return t_uint;

        default:
            return x;
    }
}

// ...
prim_type prim_from_str(char* s) {
    if (strcmp(s, "i8")==0) return t_i8;
    else if (strcmp(s, "i16")==0) return t_i16;
    else if (strcmp(s, "i32")==0) return t_i32;
    else if (strcmp(s, "i64")==0) return t_i64;
    else if (strcmp(s, "u8")==0) return t_u8;
    else if (strcmp(s, "u16")==0) return t_u16;
    else if (strcmp(s, "u32")==0) return t_u32;
    else if (strcmp(s, "u64")==0) return t_u64;
    else if (strcmp(s, "f8")==0) return t_f8;
    else if (strcmp(s, "f16")==0) return t_f16;
    else if (strcmp(s, "f32")==0) return t_f32;
    else if (strcmp(s, "f64")==0) return t_f64;
    else if (strcmp(s, "bool")==0) return t_bool;
    else if (strcmp(s, "str")==0) return t_str;
    else if (strcmp(s, "char")==0) return t_char;
    else if (strcmp(s, "void")==0) return t_void;
    else return t_compound;
}

typedef struct {
    prim_type prim;
    type_flags flags;
    void* compound_type;
} validate_type;

validate_type from_prim(prim_type x) {
    validate_type vt = {.prim=x, .flags=0, .compound_type=NULL};
    return vt;
}

prim_type prim_type_from_num(num* n) {
    switch (n->ty) {
        case num_decimal: return t_float;
        case num_integer: return t_int;
        case num_unsigned: return t_uint;
    }
}

void specialize_prim(validate_type* vt) {
    switch (vt->prim) {
        case t_int: vt->prim = t_i32;
        case t_uint: vt->prim = t_u32;
        case t_float: vt->prim = t_f32;

        default:;
    }
}

void implicit_cast_prim(validate_type* vt1, validate_type* vt2) {
    if (vt1->prim == vt2->prim) {
        return;
    } else if (vt1->prim == generalize_prim(vt2->prim)) {
        vt1->prim = vt2->prim;
    } else if (vt2->prim == generalize_prim(vt1->prim)) {
        vt2->prim = vt1->prim;
    }
}

/// compares from and to with the premise that from is being coerced into to
int type_eq(validate_type* from, validate_type* to) {
    return from->prim == to->prim
           && (from->flags == to->flags //if either flags==flags or flags==flags without const
           || from->flags == (to->flags & ~ty_const)); //TODO: compare compound types
}

int type_from_id(validator* v, validate_type* vt, type_id* tid) {
    prim_type pt = prim_from_str(tid->name);
    vt->prim = pt;
    vt->flags = tid->flags;

//    if (pt==t_compound) {
//        void* x = map_find(&v->scope, tid->name);
//        if (x) {
//            vt.compound_type = x + sizeof(char*); //get value
//        } else {
//            throw(v->fe, &tid->s, "cannot find referenced type in scope");
//        }
//    }

    return 1;
}

int scope_insert(validator* v, char* name, stmt* s) {
    vector_push(&v->new_scope, &name);
    map_insert(v.)
}

int type_check_block(validator* v, validate_type* vt, block* b);

int type_check_expr(validator* v, validate_type* vt, expr* x) {
    if (x->left & left_ref) {
        vt->flags |= ty_ptr;
    }

    if (x->left & left_num) {
        vt->prim = prim_type_from_num(x->left_ref);
        vt->flags = ty_const; //literals are constant
    } else if (x->left & left_access) {
        void* var = map_find(&v->scope, x->left_ref);
        if (!var) throw(v->fe, &x->left_span, "variable not found in scope");

        stmt* var_def = var + sizeof(char*);
        switch (var_def->t) {
            case s_fn:
                vt->prim = t_func;
                vt->flags = ty_const | ty_ptr;
                break;
            case s_bind:
                type_check_block(v, vt, );
        }
    }
}

int type_check_block(validator* v, validate_type* vt, block* b) {
    vector_iterator iter = vector_iterate(&b->stmts);
    while (vector_next(&iter)) {
        if (((stmt*)iter.x)->t == s_ret) {

        }
    }
}