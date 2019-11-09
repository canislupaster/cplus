#include "term.c"

#include "vector.c"
#include "hashtable.c"

#include "str.c"
#include "string.h"
#include "math.h"

typedef struct {
    char* start;
    char* end;
} span;

typedef enum {
    s_typedef, s_fn,
    s_block, s_ret_block,
    s_bind,
    s_expr, s_ret,

    s_fn_arg, //arguments will be referenced using this "statement"

    s_unresolved_fn,
    s_unresolved_type
} stmt_t;

typedef struct {
    stmt_t t;
    span s;
    void* x;
} stmt;

typedef struct {
    enum {ENUM, STRUCT, UNION} kind;

    size_t size;
    vector members;
} parse_type;

typedef enum {
    t_any, //type used for unresolved stuffs
    t_int, t_uint, t_float,

    t_i8, t_i16, t_i32, t_i64,
    t_u8, t_u16, t_u32, t_u64,
    t_f8, t_f16, t_f32, t_f64,

    t_bool, t_str, t_char,
    t_void, t_func, t_compound
} prim_type;

typedef enum {
    ty_const = 0x01, ty_ptr = 0x02, ty_arr = 0x04
} type_flags;

typedef struct {
    prim_type prim;

    void* data;

    unsigned long size;

    type_flags flags;
} typedata;

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


char* prim_str(typedata* td) {
    //type is compound and is resolved
    if (td->prim == t_compound) {
        if (!td->data) return "(unresolved type)";
        return NULL; //TODO: typedefs
    } else {
        switch (td->prim) {
            case t_int: return "(int)";
            case t_uint: return "(uint)";
            case t_float: return "(float)";
            case t_any: return "(any)";
            case t_void: return "void";
            case t_i8: return "i8";
            case t_i16: return "i16";
            case t_i32: return "i32";
            case t_i64: return "i64";
            case t_u8: return "u8";
            case t_u16: return "u16";
            case t_u32: return "u32";
            case t_u64: return "u64";
            case t_f8: return "f8";
            case t_f16: return "f16";
            case t_f32: return "f32";
            case t_f64: return "f64";
            case t_bool: return "bool";
            case t_str: return "str";
            case t_char: return "char";
            case t_func: return "func";
        }
    }
}

char* type_str(typedata* td) {
    char* const_str = "";
    if (td->flags & ty_const) const_str = "const";

    char* ptr_str = "";
    if (td->flags & ty_ptr) ptr_str = "*";

    char* arr_str = "";
    if (td->flags & ty_arr) asprintf(&arr_str, "[%lu]", td->size);

    return isprintf("%s%s%s%s", const_str, prim_str(td), ptr_str, arr_str);
}

typedef struct {
    vector stmts;
    /// map of all declarations <-> statement pointers inside scope for validation
    /// copied into lower scopes
    /// sorry it sucks im so sorry please forgive me
    map declarations;
    /// map of all unresolved declarations <-> statement pointers for new defines
    /// copied into higher scopes
    map unresolved;
    typedata ret_type;
} block;

typedef struct {
    char* file;
    span s;
    unsigned long len;

    vector tokens;

    block global;

    /// tells whether to continue into codegen
    char errored;
} frontend;

const span SPAN_NULL = {.start=NULL};

///upper bound exclusive, returns 1 if equal
int spaneq(span s, char* x) {
    char c;

    while (!(!(c = *x++) && s.start == s.end)) {
        if (*s.start != c) {
            return 0;
        }

        s.start++;
    }

    return 1;
}

unsigned long spanlen(span* s) {
    return s->end-s->start;
}

char* spanstr(span* s) {
    char* v = malloc(spanlen(s) + 1);

    memcpy(v, s->start, spanlen(s));
    v[spanlen(s)] = 0; //set terminator

    return v;
}

void msg(frontend* fe, span* s, const char* template_empty, const char* template, const char* msg) {
    if (s->start == NULL) {
        set_col(stderr, RED);
        fprintf(stderr, template_empty, fe->file);

        set_col(stderr, WHITE);
        fprintf(stderr, "%s\n\n", msg);
    }

    //while the file is probably the same, we may have to reconstruct line and col if custom span
    unsigned long line=0;
    unsigned long col=0;

    span line_span = *s;
    //count cols backwards
    while (*line_span.start != '\n' && line_span.start >= fe->s.start) {
        col++; //add column for every time we decrement to find previous linebreak
        line_span.start--;
    }

    line_span.start++;

    //...then count up for lines
    for (char* pos=line_span.start; pos > fe->s.start; pos--) {
        if (*pos == '\n') line++;
    }

    //resolve end of lines to show, upper bound exclusive
    while (line_span.end < fe->s.end && *line_span.end != '\n') {
        line_span.end++;
    }

    //store spans of each line
    vector lines = vector_new(sizeof(span));

    char* line_start = line_span.start;
    char* pos = line_span.start;
    while (pos++, pos <= line_span.end) {
        if (pos == line_span.end || *pos == '\n') {
            //+1 to skip newline
            span* x = vector_push(&lines);
            x->start=line_start; x->end=pos;


            line_start = pos+1; //skip newline
        }
    }

    //lines start at 1
    line++;

    int digits = (int)log10(line+lines.length)+1;

    set_col(stderr, RED);
    fprintf(stderr, template, fe->file, line, col);

    set_col(stderr, WHITE);
    fprintf(stderr, "%s\n\n", msg);

    set_col(stderr, RED);
    //write lines
    for (unsigned long i=0; i<lines.length; i++) {
        char* line_num = malloc(digits+1);
        line_num[digits] = 0;

        int l_digits = (int)log10(line+i)+1;
        sprintf(line_num+(digits-l_digits), "%lu", line + i);

        if (digits-l_digits > 0) {
            //fill with whitespace
            for (unsigned ws=0; ws<digits-l_digits; ws++) line_num[ws] = ' ';
        }

        span* x = vector_get(&lines, i);

        char* str = spanstr(x);
        fprintf(stderr, "%s | %s\n", line_num, str);
        free(str);
        //check if original span is contained within line, then highlight
        if ((s->end > x->start)
            && (s->start >= x->start && s->end <= x->end)) {
            //highlight end - line start
            char* buf = malloc((s->end - x->start) + 1);
            buf[s->end-x->start] = 0;

            //fill before-highlight with whitespace
            unsigned long long ws;
            for (ws=0; ws < s->start - x->start; ws++) {
                buf[ws] = ' ';
            }

            for (unsigned long long hi=ws; hi < ws + (s->end - s->start); hi++) {
                buf[hi] = '^';
            }

            fprintf(stderr, "%s | %s\n", line_num, buf);
        }
    }

    fprintf(stderr, "\n");
}

///always returns zero for convenience
int throw(frontend* fe, span* s, const char* x) {
    fe->errored=1;
    msg(fe, s, "error in %s", "error at %s:%lu:%lu: ", x);
    return 0;
}

void warn(frontend* fe, span* s, const char* x) {
    msg(fe, s, "warning in %s", "warning at %s:%lu:%lu: ", x);
}

void note(frontend* fe, span* s, const char* x) {
    msg(fe, s, "note: in %s", "note: at %s:%lu:%lu, ", x);
}

typedef struct {
    enum {
        num_decimal,
        num_integer,
        num_unsigned
    } ty;

    /// vector of char
    vector digits;
    /// counted from right if number is a decimal
    unsigned decimal_place;
} num;

unsigned long num_to_long(num* n) {
    unsigned long x=0;

    vector_iterator iter = vector_iterate(&n->digits);
    iter.rev=1; //iterate in reverse

    while (vector_next(&iter)) {
        unsigned long i = (unsigned long)*((char*)iter.x);
        x *= i * (10^(iter.i-1));
    }

    return x;
}

void print_num(num* n) {
    vector_iterator iter = vector_iterate(&n->digits);
    while (vector_next(&iter)) {
        if (n->ty == num_decimal && n->digits.length - iter.i == n->decimal_place) {
            printf(".");
        }
        printf("%u", *(unsigned char*)iter.x);
    }
}

typedef enum {
    t_lparen, t_rparen, t_lbrace, t_rbrace, t_lidx, t_ridx,
    t_id, t_const, t_return, t_if, t_else,
    t_mul, t_ref, t_num, t_comma, t_sep, t_set,
    t_add, t_sub, t_div, t_mod,
    t_incr, t_decr,
    t_not, t_lt, t_gt, t_le, t_ge, t_eq
} token_type;

typedef struct {
    token_type tt;
    span s;
    void* val;
} token;

typedef struct {
    frontend* fe;
    span pos;
    char x;
} lexer;

int lex_eof(lexer* l) {
    return l->pos.end > l->fe->s.end;
}

int lex_next(lexer* l) {
    l->pos.end++;
    if (!lex_eof(l)) {
        l->x = *(l->pos.end-1);
        return 1;
    } else {
        return 0;
    }
}

char lex_back(lexer* l) {
    l->pos.end--;
}

/// marks current char as start
void lex_mark(lexer* l) {
    l->pos.start = l->pos.end-1;
}

/// returns null when eof
char lex_peek(lexer* l, int i) {
    if (l->pos.end+i-1 >= l->fe->s.end) return 0;
    return *(l->pos.end+i-1);
}

/// does not consume if unequal
int lex_next_eq(lexer* l, char x) {
    if (lex_peek(l, 1) == x) {
        lex_next(l); return 1;
    } else {
        return 0;
    }
}

/// utility fn
void token_push(lexer* l, token_type tt) {
    token* t = vector_push(&l->fe->tokens);
    t->tt=tt; t->s=l->pos; t->val=NULL;
}

void token_push_val(lexer* l, token_type tt, void* val, size_t size) {
    void* heap_val = malloc(size);
    memcpy(heap_val, val, size);

    token* t = vector_push(&l->fe->tokens);
    t->tt=tt; t->s=l->pos; t->val=heap_val;
}

void lex(frontend* fe) {
    lexer l;
    l.fe = fe;
    l.pos.start = l.fe->s.start;
    l.pos.end = l.pos.start;

    while (lex_next(&l)) {
        lex_mark(&l);

        switch (l.x) {
            case ' ': break; //skip whitespace
            case '\n': break; //skip newlines
            case '\r': break; //skip cr(lf)
            case '/': { //consume comments
                if (lex_peek(&l, 1) == '/') {
                    lex_next(&l);
                    while (lex_next(&l) && l.x != '\n');
                } else if (lex_peek(&l, 1) == '*') {
                    lex_next(&l);
                    while (lex_next(&l) && (l.x != '*' || lex_peek(&l, 1) != '/'));
                } else {
                    token_push(&l, t_div);
                }

                break;
            }

            case '(':
                token_push(&l, t_lparen); break;
            case ')':
                token_push(&l, t_rparen); break;
            case '{':
                token_push(&l, t_lbrace); break;
            case '}':
                token_push(&l, t_rbrace); break;
            case '[':
                token_push(&l, t_lidx); break;
            case ']':
                token_push(&l, t_ridx); break;

            case '&':
                token_push(&l, t_ref); break;
            case ',':
                token_push(&l, t_comma); break;
            case ';':
                token_push(&l, t_sep); break;
            case '!':
                token_push(&l, t_not); break;

            case '+':
                if (lex_next_eq(&l, '+')) token_push(&l, t_incr);
                else token_push(&l, t_add); break;
            case '-':
                if (lex_next_eq(&l, '+')) token_push(&l, t_decr);
                else token_push(&l, t_sub); break;
            case '*':
                token_push(&l, t_mul); break;
            case '%':
                token_push(&l, t_mod); break;

            case '>': if (lex_next_eq(&l, '=')) token_push(&l, t_ge); else token_push(&l, t_gt); break;
            case '<': if (lex_next_eq(&l, '=')) token_push(&l, t_le); else token_push(&l, t_lt); break;
            case '=': if (lex_next_eq(&l, '=')) token_push(&l, t_eq); else token_push(&l, t_set); break;

            case '.':
            case '0' ... '9': {
                num num;

                num.digits = vector_new(sizeof(char));
                num.ty = num_integer;

                do {
                    if (l.x >= '0' && l.x <= '9') {
                        unsigned char* x = vector_push(&num.digits);
                        *x = l.x - '0';
                        //increment num_decimal place
                        num.decimal_place++;
                    } else if (l.x == '.') {
                        num.ty = num_decimal;
                        num.decimal_place = 0;
                    } else if (l.x == 'u') {
                        if (num.ty == num_decimal) {
                            throw(fe, &l.pos, "num_decimal numbers cannot be marked unsigned");
                            break;
                        }

                        num.ty = num_unsigned;
                    } else {
                        lex_back(&l); //undo consuming non-number char
                        break;
                    }
                } while(lex_next(&l));

                token_push_val(&l, t_num, &num, sizeof(num));
                break;
            }

            case 'a' ... 'z':
            case 'A' ... 'Z': {
                while ((l.x = lex_peek(&l, 1)) &&
                       ((l.x >= 'a' && l.x <= 'z') || (l.x >= '0' && l.x <= '9') || (l.x == '_')) &&
                       lex_next(&l));

                char* s = spanstr(&l.pos);
                if (strcmp(s, "return")==0) token_push(&l, t_return);
                else if (strcmp(s, "if")==0) token_push(&l, t_if);
                else if (strcmp(s, "else")==0) token_push(&l, t_else);
                else if (strcmp(s, "const")==0) token_push(&l, t_const);
                else token_push_val(&l, t_id, spanstr(&l.pos), spanlen(&l.pos) + 1);

                break;
            }

            default: throw(l.fe, &l.pos, "unrecognized token");
        }
    }
}

void token_free(token* t) {
    switch (t->tt) {
        case t_id: free(t->val);
        case t_num: free(t->val);

        default:;
    }
}

///initialize frontend with file
int read_file(frontend* fe, char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    //allocate length of string
    unsigned long len = ftell(f);
    char* str = malloc(len+1);
    str[len] = 0;
    //back to beginning
    rewind(f);

    fread(str, len, 1, f);

    fe->file = filename;

    fe->s.start = str;
    fe->s.end = str+len;
    fe->len = len;

    return 1;
}

frontend make_frontend(char* file) {
    frontend fe = {.tokens=vector_new(sizeof(token))};

    read_file(&fe, file);

    return fe;
}

void fe_free(frontend* fe) {
    free(fe->s.start);

    vector_iterator i = vector_iterate(&fe->tokens);
    while (vector_next(&i)) {
        if (((token*)i.x)->val) {
            free(((token*)i.x)->val);
        }
    }

    vector_free(&fe->tokens);
}