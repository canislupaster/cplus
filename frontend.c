#include "term.c"

#include "vector.c"
#include "hashtable.c"

#include "string.h"
#include "math.h"

typedef struct {
    char* start;
    char* end;
} span;

typedef struct {
    char* file;
    span s;
    unsigned long len;

    vector tokens;
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

///returns zero for error
int throw(frontend* fe, span s, const char* msg) {
    if (s.start == NULL) {
        set_col(stderr, RED);
        fprintf(stderr, "Error in %s: ", fe->file);

        set_col(stderr, WHITE);
        fprintf(stderr, "%s\n\n", msg);

        return 0;
    }

    //while the file is probably the same, we may have to reconstruct line and col if custom span
    unsigned long line=0;
    unsigned long col=0;

    //count cols backwards
    while (*s.start != '\n' && s.start > fe->s.start) {
        col++; //add column for every time we decrement to find previous linebreak
        s.start--;
    }

    //...then count up for lines
    for (char* pos=s.start; pos > fe->s.start; pos--) {
        if (*pos == '\n') line++;
    }

    //resolve end of lines to show, upper bound exclusive
    while (s.end <= fe->s.end && *s.end != '\n') {
        s.end++;
    }

    //store spans of each line
    vector lines = vector_new(sizeof(span));

    char* line_start = s.start;
    char* pos = s.start;
    while (pos++, pos <= s.end) {
        if (pos == s.end || *pos == '\n') {
            //+1 to skip newline
            span x = {.start=line_start, .end=pos};
            vector_push(&lines, &x);

            line_start = pos+1; //skip newline
        }
    }

    //lines start at 1
    line++;

    int digits = (int)log10(line+lines.length)+1;

    set_col(stderr, RED);
    fprintf(stderr, "Error at %s:%lu:%lu: ", fe->file, line, col);

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

        fprintf(stderr, "%s | %s\n", line_num, spanstr(x));
        //check if original span is contained within line, then highlight
        if ((s.end > x->start)
            && ((s.start >= x->start && s.end < x->end)
                || (s.start > x->start && s.end <= x->end))) {
            //highlight end - line start
            char* buf = malloc((s.end - x->start) + 1);
            buf[s.end-x->start] = 0;

            //fill before-highlight with whitespace
            unsigned long long ws;
            for (ws=0; ws < s.start - x->start; ws++) {
                buf[ws] = ' ';
            }

            for (unsigned long long hi=ws; hi < ws + (s.end - s.start); hi++) {
                buf[hi] = '^';
            }

            fprintf(stderr, "%s | %s\n", line_num, buf);
        }
    }

    return 0;
}

typedef struct {
    enum {
        decimal,
        integer,
        unsigned_t
    } ty;

    /// vector of char
    vector digits;
    /// counted from right if number is a decimal
    unsigned decimal_place;
} num;

typedef enum {
    t_lparen, t_rparen, t_lbrace, t_rbrace, t_lidx, t_ridx,
    t_id, t_deref, t_ref, t_num, t_comma, t_sep, t_set,
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

int eof(lexer* l) {
    return l->pos.end > l->fe->s.end;
}

int next(lexer* l) {
    l->x = *l->pos.end;
    l->pos.end++;
    return eof(l);
}

char back(lexer* l) {
    l->pos.end--;
}

void mark(lexer* l) {
    l->pos.start = l->pos.end;
}

/// returns null when eof
char peek(lexer* l, int i) {
    if (l->pos.end+i-1 >= l->fe->s.end) return 0;
    return *(l->pos.end+i-1);
}

/// does not consume if unequal
int next_eq(lexer* l, char x) {
    if (peek(l, 1) == x) {
        next(l); return 1;
    } else {
        return 0;
    }
}

/// utility fn
void push_token(lexer* l, token_type tt) {
    token t = {tt, .s=l->pos, .val=NULL};
    vector_push(&l->fe->tokens, &t);
}

void push_token_val(lexer* l, token_type tt, void* val, size_t size) {
    void* heap_val = malloc(size);
    memcpy(heap_val, val, size);

    token t = {tt, .s=l->pos, .val=heap_val};
    vector_push(&l->fe->tokens, &t);
}

void lex(frontend* fe) {
    lexer l;
    l.fe = fe;
    l.pos.start = l.fe->s.start;
    l.pos.end = l.pos.start;

    while (next(&l)) {
        mark(&l);

        switch (l.x) {
            case ' ': break; //skip whitespace
            case '\n': break; //skip newlines
            case '/': { //consume comments
                if (peek(&l, 1) == '/') {
                    next(&l);
                    while (next(&l) && l.x != '\n');
                } else if (peek(&l, 1) == '*') {
                    next(&l);
                    while (next(&l) && (l.x != '*' || peek(&l, 1) != '/'));
                }

                break;
            }

            case '(': push_token(&l, t_lparen); break;
            case ')': push_token(&l, t_rparen); break;
            case '{': push_token(&l, t_lbrace); break;
            case '}': push_token(&l, t_rbrace); break;
            case '[': push_token(&l, t_lidx); break;
            case ']': push_token(&l, t_ridx); break;

            case '*': push_token(&l, t_deref); break;
            case '&': push_token(&l, t_ref); break;
            case ',': push_token(&l, t_comma); break;
            case ';': push_token(&l, t_sep); break;
            case '!': push_token(&l, t_not); break;

            case '>': if (next_eq(&l, '=')) push_token(&l, t_ge); else push_token(&l, t_gt); break;
            case '<': if (next_eq(&l, '=')) push_token(&l, t_le); else push_token(&l, t_lt); break;
            case '=': if (next_eq(&l, '=')) push_token(&l, t_eq); else push_token(&l, t_set); break;

            case '.':
            case '0' ... '9': {
                num num;

                num.digits = vector_new(sizeof(char));
                num.ty = integer;

                uint64_t overflow_check = 0;

                do {
                    if (l.x >= '0' && l.x <= '9') {
                        unsigned char x = l.x - '0';

                        //not sure if this works
                        if (overflow_check == UINT64_MAX && x > 0) {
                            throw(fe, l.pos, "integer overflow");
                        }

                        vector_push(&num.digits, &x);

                        overflow_check *= 10; //shift left
                        overflow_check += x;
                        //increment decimal place
                        num.decimal_place++;
                    } else if (l.x == '.') {
                        num.ty = decimal;
                        num.decimal_place = 0;
                    } else if (l.x == 'u') {
                        if (num.ty == decimal) {
                            throw(fe, l.pos, "decimal numbers cannot be marked unsigned");
                            break;
                        }

                        num.ty = unsigned_t;
                    } else {
                        back(&l); //undo consuming non-number char
                        break;
                    }
                } while(next(&l));

                push_token_val(&l, t_num, &num, sizeof(num));
            }

            case 'a' ... 'z':
            case 'A' ... 'Z': {
                while ((l.x = peek(&l, 1)) &&
                    ((l.x >= 'a' && l.x <= 'z') || (l.x >= '0' && l.x <= '9') || (l.x == '_')));

                push_token_val(&l, t_id, spanstr(&l.pos), spanlen(&l.pos)+1);
            }

            default: throw(l.fe, l.pos, "unrecognized token");
        }
    }
}

/// increments pos, line, and col, and returns if the character next is last
int next(frontend* fe) {
    fe->x = *fe->pos.end;

    fe->pos.end++;

    if (*fe->pos.end == '\n') {
        fe->line++; fe->col=0;
    } else {
        fe->col++;
    }

    if (fe->pos.end <= fe->s.end) return 1;
    else return 0;
}

int take(frontend* fe, unsigned i) {
    mark(&fe->pos);

    while (i>0) {
        if (!next(fe)) return 0;
        i--;
    }

    return 1;
}

void backtrack_x(frontend* fe, char* to) {
    while (fe->pos.end > to) {
        fe->pos.end--;

        if (*fe->pos.end == '\n') {
            fe->col=0; fe->line--;
        } else {
            fe->col--;
        }
    }

    fe->x = *(fe->pos.end-1);
}

void backtrack_start(frontend* fe) {
    backtrack_x(fe, fe->pos.start);
}

/// moves cursor back, updates X, assumes i is valid
void backtrack(frontend* fe, unsigned i) {
    backtrack_x(fe, fe->pos.end-i);
}

/// is true if the current character is the last one
int eof(frontend* fe) {
    if (fe->pos.end < fe->s.end) return 0;
    else return 1;
}

/// skip whitespace, newlines, comments
int parse_skip(frontend* fe) {
    mark(&fe->pos);

    while (next(fe)) {
        if (fe->x == '/') {
            if (next(fe) && fe->x == '/') {
                //skip until eof or newline
                while (next(fe) && fe->x != '\n');
            } else {
                backtrack(fe, 1);

                return 1;
            }
        } else if (fe->x != ' ' && fe->x != '\t' && fe->x != '\n') {
            return 1;
        }
    }

    return 0;
}

/// not allowed in identifiers
const char* RESERVED = " \t\n*&\"';{}[],/\\<>=+-()!.";

/// marks start and consumes skip + id
int parse_id(frontend* fe) {
    //check first char is not numeric
    {
        parse_skip(fe);
        if ((fe->x >= '0' && fe->x <= '9') || strchr(RESERVED, fe->x)) {
            backtrack(fe, 1);
            return 0;
        } else {
            //didnt fail, mark start of id
            backtrack(fe, 1);
            mark(&fe->pos);
            next(fe);
        }
    }

    while(next(fe)) {
        if (strchr(RESERVED, fe->x)) {
            backtrack(fe, 1);
            return 1;
        }
    }

    return 1;
}

typedef struct {
    enum {ENUM, STRUCT, UNION} kind;

    size_t size;
    vector members;
} parse_type;

typedef struct {
    span s;
    //no ref, types are allowed to be defined out of order
    span name;

    char const_t;
    char ptr;
    char const_ptr;
} type_id;

int parse_type_id(frontend* fe, type_id* tid) {
    if (!parse_id(fe)) return 0;
    tid->s.start = fe->pos.start;

    if (spaneq(fe->pos, "const")) {
        tid->const_t = 1;

        if (!parse_id(fe)) return throw(fe, NULL, "expected type after const");
    } else {
        tid->const_t = 0;
    }

    tid->name = fe->pos;

    parse_skip(fe);
    if (fe->x == '*') {
        tid->ptr = 1;

        if (parse_id(fe) && spaneq(fe->pos, "const")) {
            tid->const_ptr = 1;
        } else {
            tid->const_ptr = 0;
            backtrack_start(fe);
        }
    } else {
        tid->ptr = 0;
        backtrack_start(fe);
    }

    tid->s.start = fe->pos.end;

    return 1;
}

typedef struct {
    span name;

    type_id ty;

    char func;
    vector args;
    vector stmts;
} top;

typedef enum {
    op_none, op_cast, op_sub, op_add, op_mul, op_div, op_idx
} op;

typedef enum {
    left_access, left_call, left_num, left_expr
} left;

typedef struct {
    span target;
    vector args;
} fn_call;

/// linked list
typedef struct {
    op op;

    left left;
    void* left_ref;
    //always expr
    void* right;
} expr;

void parse_set_left(expr* expr, left l, void* x, size_t size) {
    expr->left = l;
    expr->left_ref = malloc(size);
    memcpy(expr->left_ref, x, size);
}

void parse_detach(expr* x_expr) {
    expr* detached = malloc(sizeof(expr));
    memcpy(detached, x_expr, sizeof(expr));

    x_expr->left = left_expr;
    x_expr->left_ref = detached;
}

void parse_set_right(expr* x_expr, expr* r_expr) {
    x_expr->right = malloc(sizeof(expr));
    memcpy(x_expr->right, r_expr, sizeof(expr));
}

void print_num(num* n) {
    vector_iterator iter = vector_iterate(&n->digits);
    while (vector_next(&iter)) {
        if (n->ty == decimal && n->digits.length-iter.i == n->decimal_place) {
            printf(".");
        }
        printf("%u", *(unsigned char*)iter.x);
    }
}

int parse_expr(frontend* fe, expr* x_expr, unsigned op_prec) {
    num num;
    if (parse_num(fe, &num)) {
        //copy to left
        parse_set_left(x_expr, left_num, &num, sizeof(num));
    } else if (parse_id(fe)) {
        span target = fe->pos;

        parse_skip(fe);
        if (fe->x == '(') {
            fn_call call;
            call.target = target;
            call.args = vector_new(sizeof(x_expr));

            while (parse_skip(fe), fe->x != ')') {
                //no paren, go back
                backtrack(fe, 1);

                expr expr_arg;
                if (!parse_expr(fe, &expr_arg, 0)) throw(fe, NULL, "expected expression");

                //consume comma, if exists
                parse_skip(fe);
                if (fe->x != ',') {
                    backtrack(fe, 1);
                }
            }

            parse_set_left(x_expr, left_call, &call, sizeof(fn_call));
        } else if (fe->x == '[') {
            //set left to index expr
            expr index_expr;
            parse_set_left(&index_expr, left_access, &target, sizeof(span));

            expr r_expr;
            if (!parse_expr(fe, &r_expr, 0)) return throw(fe, NULL, "expected expression");
            parse_set_right(&index_expr, &r_expr);

            if (!next(fe) || fe->x!=']') return throw(fe, NULL, "expected right bracket to end index");

            parse_set_left(x_expr, left_expr, &index_expr, sizeof(expr));
        } else {
            //not call or index, simple access
            parse_set_left(x_expr, left_access, &target, sizeof(span));

            backtrack_start(fe); //neither ( or [, dont consume
        }
    } else {
        return 0;
    }

    //parse ops
    x_expr->op = op_none;

    while(1) {
        op x_op;

        if (!parse_skip(fe)) return 1;

        unsigned x_op_prec;

        switch (fe->x) {
            case '+': {
                x_op = op_add;
                x_op_prec = 10;
            }
                break;

            case '-': {
                x_op = op_sub;
                x_op_prec = 10;
            }
                break;

            case '/': {
                x_op = op_div;
                x_op_prec = 20;
            }
                break;

            case '*': {
                x_op = op_mul;
                x_op_prec = 20;
            }
                break;

            default: {
                backtrack_start(fe);
                return 1;
            }
        }

        //dont parse any more
        if (x_op_prec < op_prec) {
            backtrack_start(fe);
            return 1;
        }

        //op already exists, detach
        //happens every time after first loop since each loop sets op
        if (x_expr->op != op_none) {
            parse_detach(x_expr);
        }

        x_expr->op = x_op;

        expr r_expr;
        if (!parse_expr(fe, &r_expr, x_op_prec)) return throw(fe, NULL, "expected expression after operator");
        parse_set_right(x_expr, &r_expr);
    }
}

void print_expr(expr* e) {
    switch (e->left) {
        case left_num: print_num((num*)e->left_ref); break;
        case left_expr: print_expr((expr*)e->left_ref); break;
        case left_access: {
            char* s = spanstr((span*)e->left_ref);
            printf("access %s", s);
            free(s);
            break;
        }
        case left_call: {
            char* s = spanstr(&((fn_call*)e->left_ref)->target);
            printf("call %s", s);
            free(s);
            break;
        }
    }

    switch (e->op) {
        case op_add: printf(" + ("); print_expr(e->right); printf(")"); break;
        case op_div: printf(" / ("); print_expr(e->right); printf(")"); break;
        case op_mul: printf(" * ("); print_expr(e->right); printf(")"); break;
        case op_sub: printf(" - ("); print_expr(e->right); printf(")"); break;
        case op_idx: printf("["); print_expr(e->right); printf("]"); break;
        default: return;
    }
}

typedef struct {
    span name;
    type_id ty;
} arg;

int parse_top(frontend* fe, top* v) {
    type_id x;
    if (!parse_type_id(fe, &x)) return 0;
    v->ty = x;

    if (!parse_id(fe)) return throw(fe, NULL, "expected identifier");
    v->name = fe->pos;

    if (!parse_skip(fe)) return throw(fe, NULL, "expected function or item definition");

    if (fe->x == '(') {
        v->func = 1;
        v->args = vector_new(sizeof(arg));

        //similar to parse_expr's fn call handling
        while (parse_skip(fe), fe->x != ')') {
            //no paren, go back
            backtrack(fe, 1);

            type_id arg_t;

            if (!parse_type_id(fe, &arg_t)) return throw(fe, NULL, "expected argument type");
            if (!parse_id(fe)) return throw(fe, NULL, "expected argument identifier");

            arg arg = {.name=fe->pos, .ty=arg_t};
            vector_push(&v->args, &arg);

            //consume comma, if exists
            parse_skip(fe);
            if (fe->x != ',') {
                backtrack(fe, 1);
            }
        }
    } else if (fe->x != '=') {
        return throw(fe, NULL, "expected ( or =");
    }

    parse_skip(fe);
    if (fe->x == '{') {
        parse_skip(fe);

    } else {

    }

    map* scope = vector_get(&fe->scope, fe->scope.length-1);

    char* name_s = spanstr(&v->name);
    map_insert(scope, name_s, &v);
    free(name_s);

    return 1;
}

///initialize frontend with file
int read_file(frontend* fe, char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    //allocate length of string
    unsigned long len = ftell(f);
    char* str = malloc(len);
    //back to beginning
    rewind(f);

    fread(str, len, 1, f);

    fe->file = filename;

    fe->s.start = str;
    fe->s.end = str+len;
    fe->len = len;

    fe->pos.start=str;
    fe->pos.end=str;

    fe->line=0;
    fe->col=0;

    return 1;
}

frontend make_frontend(char* file) {
    frontend fe = {
        .x=0,
        .scope=vector_new(sizeof(map))
    };

    map x = map_new();
    map_configure_string_key(&x, sizeof(top));
    //copy to heap...
    vector_push(&fe.scope, &x);

    read_file(&fe, file);

    return fe;
}

void fe_free(frontend* fe) {
    free(fe->s.start);

    vector_iterator i = vector_iterate(&fe->scope);
    while (vector_next(&i)) {
        map_free(i.x);
    }

    vector_free(&fe->scope);
}