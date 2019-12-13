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

/// statements are expressions, blocks, or bindings that are each distinct, have a span, and have a type
typedef enum {
    s_typedef, s_fn,
    s_block, s_ret_block,
    s_bind,
    s_expr, s_ret,

    s_fn_arg, //arguments will be referenced using this "statement"
} stmt_t;

typedef enum {
    ty_any, //type used for unresolved stuffs
    ty_meta, //generalizes meta types
    ty_int, ty_uint, ty_float, t_ptr, //t_ptr used when generalizing a ptr type

    ty_i8, ty_i16, ty_i32, ty_i64,
    ty_u8, ty_u16, ty_u32, ty_u64,
    ty_f8, ty_f16, ty_f32, ty_f64,

    ty_bool, ty_str, ty_char,
    ty_void, ty_func, ty_compound,
} prim_type;

typedef enum {
    ty_f_meta = 0x01, //it would be cleaner to have typedata referenced in the data attribute in meta types, but that is slow and hard
    ty_const = 0x02, ty_ptr = 0x04, ty_arr = 0x08, ty_inline = 0x10
} type_flags;

typedef struct {
    prim_type prim;

    void* data;

    unsigned long size;

    type_flags flags;
} typedata;

typedef struct {
    stmt_t t;
    span s;
    void* x;
} stmt;

// ...
prim_type prim_from_str(char* s) {
    if (strcmp(s, "i8")==0) return ty_i8;
    else if (strcmp(s, "i16")==0) return ty_i16;
    else if (strcmp(s, "i32")==0) return ty_i32;
    else if (strcmp(s, "i64")==0) return ty_i64;
    else if (strcmp(s, "u8")==0) return ty_u8;
    else if (strcmp(s, "u16")==0) return ty_u16;
    else if (strcmp(s, "u32")==0) return ty_u32;
    else if (strcmp(s, "u64")==0) return ty_u64;
    else if (strcmp(s, "f8")==0) return ty_f8;
    else if (strcmp(s, "f16")==0) return ty_f16;
    else if (strcmp(s, "f32")==0) return ty_f32;
    else if (strcmp(s, "f64")==0) return ty_f64;
    else if (strcmp(s, "bool")==0) return ty_bool;
    else if (strcmp(s, "str")==0) return ty_str;
    else if (strcmp(s, "char")==0) return ty_char;
    else if (strcmp(s, "void")==0) return ty_void;
    else return ty_compound;
}


char* prim_str(typedata* td) {
    //type is compound and is resolved
    if (td->prim == ty_compound) {
        if (!td->data) return "(unresolved type)";
        return NULL; //TODO: typedefs
    } else {
        switch (td->prim) {
            case ty_int: return "(int)";
            case ty_uint: return "(uint)";
            case ty_float: return "(float)";
            case t_ptr: return "(ptr)";
            case ty_any: return "(any)";
            case ty_void: return "void";
            case ty_meta: return "type";
            case ty_i8: return "i8";
            case ty_i16: return "i16";
            case ty_i32: return "i32";
            case ty_i64: return "i64";
            case ty_u8: return "u8";
            case ty_u16: return "u16";
            case ty_u32: return "u32";
            case ty_u64: return "u64";
            case ty_f8: return "f8";
            case ty_f16: return "f16";
            case ty_f32: return "f32";
            case ty_f64: return "f64";
            case ty_bool: return "bool";
            case ty_str: return "str";
            case ty_char: return "char";
            case ty_func: return "func";
        }
    }
}

char* type_str(typedata* td) {
    char* const_str = "";
    //only use const if ptr, otherwise having const is insignificant
    if (td->flags & ty_const && td->flags & ty_ptr) const_str = "const ";

    char* ptr_str = "";
    if (td->flags & ty_ptr) ptr_str = "*";

    char* arr_str = "";
    if (td->flags & ty_arr) asprintf(&arr_str, "[%lu]", td->size);

    return isprintf("%s%s%s%s", const_str, prim_str(td), ptr_str, arr_str);
}

typedef struct {
    vector stmts;
    /// map of all declarations <-> statement pointers inside scope for validation
    map declarations;
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

void msg(frontend* fe, span* s, char color1, char color2, const char* template_empty, const char* template, const char* msg) {
    if (s->start == NULL) {
        set_col(stderr, color1);
        fprintf(stderr, template_empty, fe->file);

        set_col(stderr, color2);
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

    set_col(stderr, color1);
    fprintf(stderr, template, fe->file, line, col);

    set_col(stderr, color2);
    fprintf(stderr, "%s\n\n", msg);

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
        set_col(stderr, color1);
        fprintf(stderr, "%s | ", line_num);
        set_col(stderr, color2);
        fprintf(stderr, "%s\n", str);
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

            set_col(stderr, color1);
            fprintf(stderr, "%s | %s\n", line_num, buf);
        }
    }

    fprintf(stderr, "\n");
}

///always returns zero for convenience
int throw(frontend* fe, span* s, const char* x) {
    fe->errored=1;
    msg(fe, s, RED, WHITE, "error in %s", "error at %s:%lu:%lu: ", x);
    return 0;
}

void warn(frontend* fe, span* s, const char* x) {
    msg(fe, s, YELLOW, WHITE, "warning in %s", "warning at %s:%lu:%lu: ", x);
}

void note(frontend* fe, span* s, const char* x) {
    msg(fe, s, GRAY, WHITE, "note: in %s", "note: at %s:%lu:%lu, ", x);
}

void scope_insert(frontend* fe, map* scope, char* name, stmt* s) {
    stmt* x = map_find(scope, &name);
    if (x) {
        throw(fe, &s->s, "name already used");
        note(fe, &x->s, "declared here");
    }
    //still inserted idk why
    map_insertcpy(scope, &name, s);
}

typedef struct {
    enum {
        num_decimal,
        num_integer,
        num_unsigned
    } ty;

    union {
        uint64_t uint;
        int64_t integer;
        double decimal;
    };
} num;

void print_num(num* n) {
    switch (n->ty) {
        case num_decimal: printf("%f", n->decimal); break;
        case num_integer: printf("%lli", n->integer); break;
        case num_unsigned: printf("%llu", n->uint); break;
    }
}

typedef enum {
    t_lparen, t_rparen, t_lbrace, t_rbrace, t_lidx, t_ridx,
    t_id, t_const, t_inline, t_return, t_if, t_else,
    t_mul, t_ref, t_num, t_comma, t_sep, t_set,
    t_char, t_str,
    t_add, t_sub, t_div, t_mod,
    t_incr, t_decr,
    t_not, t_lt, t_gt, t_le, t_ge, t_eq,
    t_eof
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
char lex_peek(lexer* l) {
    if (l->pos.end-1 >= l->fe->s.end) return 0;
    return *(l->pos.end);
}

/// does not consume if unequal
int lex_next_eq(lexer* l, char x) {
    if (lex_peek(l) == x) {
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
                if (lex_peek(&l) == '/') {
                    lex_next(&l);
                    while (lex_next(&l) && l.x != '\n');
                } else if (lex_peek(&l) == '*') {
                    lex_next(&l);
                    while (lex_next(&l) && (l.x != '*' || lex_peek(&l) != '/'));
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

            //parse character
            case '\'': {
                if (lex_next(&l) && l.x == '\\' && lex_next(&l)) { //parse escaped
                    token_push_val(&l, t_char, &l.x, sizeof(char));
                } else { //parse unescaped
                    if (!lex_next(&l) || l.x == '\'') {
                        throw(fe, &l.pos, "expected character for character literal");
                        break;
                    }

                    token_push_val(&l, t_char, &l.x, sizeof(char));
                }

                if (lex_next(&l) && l.x != '\'') {
                    throw(fe, &l.pos, "expected end-quote after character");
                    lex_back(&l);
                    break;
                }

                break;
            }

            //parse string
            case '\"': {
                span str_data = {.start=l.pos.end};
                char escaped=0;

                while (1) {
                    if (!lex_next(&l)) {
                        str_data.end = l.pos.end;
                        throw(fe, &l.pos, "expected end of string");
                        break;
                    }

                    if (l.x == '\"' && !escaped) {
                        str_data.end = l.pos.end;
                        break;
                    }

                    if (l.x == '\\' && !escaped) {
                        escaped=1;
                    } else {
                        escaped=0;
                    }
                }

                token_push_val(&l, t_str, spanstr(&str_data), spanlen(&str_data) + 1);
                break;
            }

            case '.':
            case '0' ... '9': {
                num num;

                unsigned long decimal_place=0;
                num.ty = num_integer;
                num.uint = 0;

                do {
                    if (l.x >= '0' && l.x <= '9') {
                        int val = l.x - '0';
                        //modify decimal or uint
                        if (num.ty == num_decimal) {
                            num.decimal += (double)val / (10^decimal_place);
                        } else {
                            uint64_t old_val = num.uint;

                            num.uint *= 10;
                            num.uint += val;

                            if (num.uint < old_val) {
                                throw(fe, &l.pos, "integer overflow");
                                break;
                            }
                        }
                        //increment num_decimal place
                        decimal_place++;
                    } else if (l.x == '.') {
                        if (num.ty == num_decimal) { // already marked decimal
                            throw(fe, &l.pos, "decimal numbers cannot have multiple dots");
                            break;
                        }

                        num.ty = num_decimal;
                        num.decimal = (double)num.uint;
                        decimal_place = 0;
                    } else if (l.x == 'u') {
                        if (num.ty == num_decimal) {
                            throw(fe, &l.pos, "decimal numbers cannot be marked unsigned");
                            break;
                        }

                        num.ty = num_unsigned;
                    } else {
                        lex_back(&l); //undo consuming non-number char
                        break;
                    }
                } while(lex_next(&l));

                if (num.ty == num_integer) {
                    //convert uint to integer
                    num.integer = (int64_t)num.uint;

                    if ((uint64_t)num.integer < num.uint) {
                        throw(fe, &l.pos, "integer overflow");
                    }
                }

                token_push_val(&l, t_num, &num, sizeof(num));
                break;
            }

            case 'a' ... 'z':
            case 'A' ... 'Z': {
                while ((l.x = lex_peek(&l)) &&
                       ((l.x >= 'a' && l.x <= 'z') || (l.x >= '0' && l.x <= '9') || (l.x == '_')) &&
                       lex_next(&l));

                char* s = spanstr(&l.pos);
                if (strcmp(s, "return")==0) token_push(&l, t_return);
                else if (strcmp(s, "if")==0) token_push(&l, t_if);
                else if (strcmp(s, "else")==0) token_push(&l, t_else);
                else if (strcmp(s, "const")==0) token_push(&l, t_const);
                else if (strcmp(s, "inline")==0) token_push(&l, t_inline);
                else token_push_val(&l, t_id, spanstr(&l.pos), spanlen(&l.pos) + 1);

                break;
            }

            default: throw(l.fe, &l.pos, "unrecognized token");
        }
    }

    token_push(&l, t_eof); //push eof token
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

void frontend_free(frontend* fe) {
    free(fe->s.start);

    vector_iterator i = vector_iterate(&fe->tokens);
    while (vector_next(&i)) {
        token_free(i.x);
    }

    vector_free(&fe->tokens);
}