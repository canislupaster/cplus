#include "parser.c"

typedef enum {
    i_add, i_sub
} instruction;

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

typedef struct {
    prim_type prim;
    type_flags flags;
    void* compound_type;
} validate_type;

int t_generalize(validate_type* x) {
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
    char** x = vector_push(&v->new_scope);
    *x = name;

    map_insert(&v->scope, &name, &s);
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