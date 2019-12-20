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

typedef struct {
    char* qualifier;
    char* x;
} name;

typedef struct {
    enum {
        num_decimal,
        num_integer,
    } ty;

    union {
        uint64_t uint;
        int64_t integer;
        double decimal;
    };
} num;

typedef enum {
    left_expr,
    left_for,
    left_access,
    left_bind,
    left_range,
    left_num,
    left_str,
} left_t;

typedef enum {
    //prefix ops
    left_neg = 0x1,
    left_add = 0x2
} left_flags;

/// substituted in reverse
typedef struct {
    vector substitutions; //expression for every substitute indexes
} substitution;

/// linked list
typedef struct s_expr {
    left_t left;
    left_flags flags;

    span span;

    union {
        num* num;
        char* str;
        struct s_access* access;
        unsigned long bind;
        struct s_expr* expr;
        struct s_for* fore;
    } val;

    /// the different ways of matching
    /// unapplied: no identifiers used for substitution
    /// apply_bind: applier is bound to the name
    /// applied_bind: appliers' substitutes are bound if the appliers match
    enum { unapplied, apply_bind, applied } apply;
    union { name* bind; struct s_id* id; } applier;
    substitution substitutes;
} expr;

/// simplified expression form: expressions are parsed and partially* matched by exprs but reduced to simples
/// * sometimes matching calls for reduction
typedef struct s_simple {
    struct s_simple* first;

    enum {
        simple_add, simple_invert,
        simple_bind, simple_num, simple_inner
    } kind;

    union { num* by; unsigned long bind; struct s_simple* inner; } val;
} simple;

/// identifier or simple (empty vec and map)
typedef struct s_value {
    vector substitutes;
    map substitute_idx;

    simple val;
} value;

typedef struct s_id {
    span s;
    char* name;
    value val;
    span substitutes;
    unsigned precedence;
} id;

typedef struct s_for {
    expr i;
    char* name;
    expr base;

    expr x;

    char boolean;
    value gradient;
} for_expr;

//identifier, substitute, for, or id: anything named
typedef struct s_access {
    enum { a_id, a_for, a_sub, a_unbound } res;
    union { id* id; for_expr* fore; unsigned long idx; } val;
} access;

typedef struct {
    map ids;
} module;

typedef struct {
    char* file;
    span s;
    unsigned long len;

    vector tokens;

    module global;

    /// tells whether to continue into codegen
    char errored;
} frontend;

frontend* FRONTEND; //global frontend

const span SPAN_NULL = {.start=NULL};

uint64_t hash_name(name* x) {
    uint64_t qhash = x->qualifier ? hash_string(&x->qualifier) : 0;
    uint64_t nhash = hash_string(&x->x);
    
    return qhash + nhash;
}

int compare_name(name* x1, name* x2) {
    return !(strcmp(x1->qualifier, x2->qualifier)==0 && strcmp(x1->x, x2->x)==0);
}

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

void* heap(size_t size);

char* spanstr(span* s) {
    char* v = heap(spanlen(s) + 1);

    memcpy(v, s->start, spanlen(s));
    v[spanlen(s)] = 0; //set terminator

    return v;
}

void msg(frontend* fe, const span* s, char color1, char color2, const char* template_empty, const char* template, const char* msg) {
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
        char* line_num = heap(digits+1);
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
            char* buf = heap((s->end - x->start) + 1);
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
int throw(const span* s, const char* x) {
    FRONTEND->errored=1;
    msg(FRONTEND, s, RED, WHITE, "error in %s", "error at %s:%lu:%lu: ", x);
    return 0;
}

void warn(const span* s, const char* x) {
    msg(FRONTEND, s, YELLOW, WHITE, "warning in %s", "warning at %s:%lu:%lu: ", x);
}

void note(const span* s, const char* x) {
    msg(FRONTEND, s, GRAY, WHITE, "note: in %s", "note: at %s:%lu:%lu, ", x);
}

void* heap(size_t size) {
    void* res = malloc(size);

    if (!res) {
        throw(&SPAN_NULL, "out of memory!");
        exit(1);
    }

    return res;
}

void* heapcpy(size_t size, const void* val) {
    void* res = heap(size);
    memcpy(res, val, size);
    return res;
}

void print_num(num* n) {
    switch (n->ty) {
        case num_decimal: printf("%f", n->decimal); break;
        case num_integer: printf("%lli", n->integer); break;
    }
}

typedef enum {
    t_name, t_non_bind,
    t_add, t_sub,
    t_ellipsis, t_comma,
    t_in, t_for,
    t_eq, t_paren,
    t_str, t_num,
    t_sync, t_eof
} token_type;

typedef struct {
    token_type tt;
    span s;

    union {
        name* name;
        char* str;
        num* num;
    } val;
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

void lex_back(lexer* l) {
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
token* token_push(lexer* l, token_type tt) {
    token* t = vector_push(&l->fe->tokens);
    t->tt=tt; t->s=l->pos;
    return t;
}

const char* RESERVED = " \n\r/(),+-=\"+-";
name ADD_NAME = {.qualifier=NULL, .x="+"};
name SUB_NAME = {.qualifier=NULL, .x="-"};

void lex_name(lexer* l) {
    name n;
    n.qualifier = NULL;

    while ((l->x = lex_peek(l)) && strchr(RESERVED, l->x)==NULL) {
        if (l->x == '.') {
            n.qualifier = spanstr(&l->pos);
            lex_mark(l);
        }

        lex_next(l);
    }

    if (!n.qualifier && spaneq(l->pos, "for")) {
        token_push(l, t_for);
    } else {
        if (spanlen(&l->pos) == 0) {
            throw(&l->pos, "name required after qualifier");

            n.x = n.qualifier;
            n.qualifier = NULL;
        } else {
            n.x = spanstr(&l->pos);
        }

        token_push(l, t_name)->val.name = heapcpy(sizeof(name), &n);
    }
}

int lex_char(lexer* l) {
    if (!lex_next(l)) {
        token_push(l, t_eof); //push eof token
        return 0;
    }

    lex_mark(l);

    switch (l->x) {
        case ' ': break; //skip whitespace
        case '\n': {
            if (lex_peek(l) != ' ' && lex_peek(l) != '\t') token_push(l, t_sync);
            break; //newlines are synchronization tokens
        }
        case '\r': break; //skip cr(lf)
        case '/': { //consume comments
            if (lex_peek(l) == '/') {
                lex_next(l);
                while (lex_next(l) && l->x != '\n');
                
                break;
            } else if (lex_peek(l) == '*') {
                lex_next(l);
                while (lex_next(l) && (l->x != '*' || lex_peek(l) != '/'));
                
                break;
            } else {
                lex_name(l); break;
            }
        }

        case '.': {
            if (lex_next_eq(l, '.') && lex_next_eq(l, '.')) { //try consume two more dots
                token_push(l, t_ellipsis);
                break;
            } else {
                lex_name(l); break;
            }
        }

        case '(': {
            token* lparen = token_push(l, t_paren);
            
            while (!lex_next_eq(l, ')')) {
                if (!lex_char(l)) {
                    throw(&l->pos, "expected matching parenthesis");
                    note(&lparen->s, "other parenthesis here");
                    break;
                }
            }

            token_push(l, t_paren);
            break;
        }

        case ',':
            token_push(l, t_comma); break;

        case '+': token_push(l, t_add)->val.name = &ADD_NAME; break;
        case '-': token_push(l, t_sub)->val.name = &SUB_NAME; break;
        case '=': token_push(l, t_eq); break;

        //parse string
        case '\"': {
            span str_data = {.start=l->pos.end};
            char escaped=0;

            while (1) {
                if (!lex_next(l)) {
                    str_data.end = l->pos.end;
                    throw(&l->pos, "expected end of string");
                    break;
                }

                if (l->x == '\"' && !escaped) {
                    str_data.end = l->pos.end;
                    break;
                }

                if (l->x == '\\' && !escaped) {
                    escaped=1;
                } else {
                    escaped=0;
                }
            }

            token_push(l, t_str)->val.str = spanstr(&str_data);
            break;
        }

        case '0' ... '9': {
            num number;

            unsigned long decimal_place=0;
            number.ty = num_integer;
            number.uint = 0;

            do {
                if (l->x >= '0' && l->x <= '9') {
                    int val = l->x - '0';
                    //modify decimal or uint
                    if (number.ty == num_decimal) {
                        number.decimal += (double)val / (10^decimal_place);
                    } else {
                        uint64_t old_val = number.uint;

                        number.uint *= 10;
                        number.uint += val;

                        if (number.uint < old_val) {
                            throw(&l->pos, "integer overflow");
                            break;
                        }
                    }
                    //increment num_decimal place
                    decimal_place++;
                } else if (l->x == '.') {
                    if (number.ty == num_decimal) { // already marked decimal
                        throw(&l->pos, "decimal numbers cannot have multiple dots");
                        break;
                    }

                    number.ty = num_decimal;
                    number.decimal = (double)number.uint;
                    decimal_place = 0;
                } else {
                    lex_back(l); //undo consuming non-number char
                    break;
                }
            } while(lex_next(l));

            if (number.ty == num_integer) {
                //convert uint to integer
                number.integer = (int64_t)number.uint;

                if ((uint64_t)number.integer < number.uint) {
                    throw(&l->pos, "integer overflow");
                }
            }

            token_push(l, t_num)->val.num = heapcpy(sizeof(num), &number);

            break;
        }

        default: lex_name(l);
    }

    return 1;
}

void lex(frontend* fe) {
    lexer l = {.fe=fe, .pos={.start=fe->s.start, .end=fe->s.start}};
    while (lex_char(&l));
}

void token_free(token* t) {
    switch (t->tt) {
        case t_name: {
            if (t->val.name->qualifier) free(t->val.name->qualifier);
            free(t->val.name->x);
        }
        case t_num: free(t->val.num); break;

        default:;
    }
}

int is_name(token* x) {
    return x->tt == t_name || x->tt == t_add || x->tt == t_sub;
}

///initialize frontend with file
int read_file(frontend* fe, char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    //allocate length of string
    unsigned long len = ftell(f);
    char* str = heap(len+1);
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
    FRONTEND = &fe;

    read_file(&fe, file);

    return fe;
}

void expr_free(expr* e) {
    switch (e->left) {
        case left_expr: {
            expr_free(e->val.expr);
            free(e->val.expr);
            break;
        }
        case left_for: {
            expr_free(&e->val.fore->base);
            expr_free(&e->val.fore->i);
            expr_free(&e->val.fore->x);
        }
        default:;
    }

    vector_iterator iter = vector_iterate(&e->substitutes);
    while (vector_next(&iter)) {
        expr_free(iter.x);
    }

    vector_free(&e->substitutes);
}

void value_free(value* val) {
    vector_free(&val->substitutes);
    map_free(&val->substitute_idx);
    expr_free(val->val);
}

void id_free(id* xid) {
    value_free(&xid->val);
}

void module_free(module* b) {
    map_iterator iter = map_iterate(&b->ids);
    while (map_next(&iter)) {
        id_free(*(id**)iter.x);
    }

    map_free(&b->ids);
}

void frontend_free(frontend* fe) {
    module_free(&fe->global);

    free(fe->s.start);

    vector_iterator i = vector_iterate(&fe->tokens);
    while (vector_next(&i)) {
        token_free(i.x);
    }

    vector_free(&fe->tokens);
}