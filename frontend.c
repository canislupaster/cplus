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
            span x = {.start=line_start, .end=pos};
            vector_push(&lines, &x);

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

        fprintf(stderr, "%s | %s\n", line_num, spanstr(x));
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
}

///always returns zero for convenience
int throw(frontend* fe, span* s, const char* x) {
    msg(fe, s, "error in %s", "error at %s:%lu:%lu: ", x);
    return 0;
}

void note(frontend* fe, span* s, const char* x) {
    msg(fe, s, "note: in %s", "note: at %s:%lu:%lu, ", x);
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
    t_id, t_mul, t_ref, t_num, t_comma, t_sep, t_set,
    t_add, t_sub, t_div, t_mod,
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
    l->x = *l->pos.end;
    l->pos.end++;
    return !lex_eof(l);
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
    token t = {tt, .s=l->pos, .val=NULL};
    vector_push(&l->fe->tokens, &t);
}

void token_push_val(lexer* l, token_type tt, void* val, size_t size) {
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
                token_push(&l, t_add); break;
            case '-':
                token_push(&l, t_sub); break;
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
                num.ty = integer;

                do {
                    if (l.x >= '0' && l.x <= '9') {
                        unsigned char x = l.x - '0';

                        vector_push(&num.digits, &x);
                        //increment decimal place
                        num.decimal_place++;
                    } else if (l.x == '.') {
                        num.ty = decimal;
                        num.decimal_place = 0;
                    } else if (l.x == 'u') {
                        if (num.ty == decimal) {
                            throw(fe, &l.pos, "decimal numbers cannot be marked unsigned");
                            break;
                        }

                        num.ty = unsigned_t;
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

                token_push_val(&l, t_id, spanstr(&l.pos), spanlen(&l.pos) + 1);

                break;
            }

            default: throw(l.fe, &l.pos, "unrecognized token");
        }
    }
}

void free_token(token* t) {
    switch (t->tt) {
        case t_id: free(t->val);
        case t_num: free(t->val);

        default:;
    }
}

typedef struct {
    frontend* fe;
    unsigned long pos;
    token x;
} parser;

int parse_eof(parser* p) {
    return p->pos > p->fe->len;
}

int parse_next(parser* p) {
    token* x = vector_get(&p->fe->tokens, p->pos);;
    if(x) p->x = *x;
    p->pos++;
    return !parse_eof(p);
}

token* parse_peek(parser* p, int i) {
    if (p->pos+i-1 >= p->fe->len) return NULL;
    else return vector_get(&p->fe->tokens, p->pos+i-1);
}

int parse_next_eq(parser* p, token_type tt) {
    token* x = parse_peek(p, 1);
    if (x && x->tt == tt) return parse_next(p);
    else return 0;
}

typedef struct {
    enum {ENUM, STRUCT, UNION} kind;

    size_t size;
    vector members;
} parse_type;

typedef struct {
    span s;
    //no ref, types are allowed to be defined out of order
    char* name;

    char const_t;
    char ptr;
    char const_ptr;
} type_id;

int parse_type_id(parser* p, type_id* tid) {
    if (!parse_next_eq(p, t_id)) return 0;
    tid->s.start = p->x.s.start;

    if (strcmp(p->x.val, "const")==0) {
        tid->const_t = 1;

        if (!parse_next_eq(p, t_id)) return throw(p->fe, &p->x.s, "expected type after const");
    } else {
        tid->const_t = 0;
    }

    tid->name = p->x.val;

    if (parse_next_eq(p, t_mul)) {
        tid->ptr = 1;

        token* const_ptr = parse_peek(p, 1);
        if (const_ptr->tt==t_id && strcmp(const_ptr->val, "const")==0) {
            tid->const_ptr = 1;
            //only consume if ==const
            parse_next(p);
        } else {
            tid->const_ptr = 0;
        }
    } else {
        tid->ptr = 0;
    }

    tid->s.end = p->x.s.end;

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
    span s;
    char* target;
    vector args;
} fn_call;

///// linked list
typedef struct {
    op op;

    left left;
    void* left_ref;
    //always expr
    void* right;
} expr;

void parse_set_left(expr* expr, left l, void* x) {
    expr->left = l;
    expr->left_ref = x;
}

void parse_detach(expr* x_expr) {
    expr* detached = malloc(sizeof(expr));
    memcpy(detached, x_expr, sizeof(expr));

    x_expr->left = left_expr;
    x_expr->left_ref = detached;
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

int parse_expr(parser* p, expr* x_expr, unsigned op_prec); //foward decl

int parse_left_expr(parser* p, expr* x_expr) {
    if (parse_next_eq(p, t_num)) {
        //copy to left
        parse_set_left(x_expr, left_num, p->x.val);
    } else if (parse_next_eq(p, t_id)) {
        char* target = p->x.val; //record starting paren
        span* target_span = &p->x.s;

        if (parse_next_eq(p, t_lparen)) {
            span* fn_paren = &p->x.s; //TODO: parenthesized procedures

            fn_call* f = malloc(sizeof(fn_call));
            f->args = vector_new(sizeof(x_expr));

            f->target = target;
            f->s.start = target_span->start;

            while (!parse_next_eq(p, t_rparen) && !parse_eof(p)) {
                expr expr_arg;
                if (!parse_expr(p, &expr_arg, 0))  return throw(p->fe, &p->x.s, "expected expression");

                //consume comma, if exists
                parse_next_eq(p, t_comma);
            }

            if (parse_eof(p)) {
                free(f);
                throw(p->fe, &p->x.s, "expected matching paren for function call");
                note(p->fe, fn_paren, "other paren here");

                return 0;
            }

            f->s.end = target_span->end;

            parse_set_left(x_expr, left_call, f);
        } else if (parse_next_eq(p, t_lidx)) {
            span* l_idx = &p->x.s; //record starting bracket
            //set left to index expr
            expr* index_expr = malloc(sizeof(expr));
            parse_set_left(index_expr, left_access, target);

            expr* r_expr = malloc(sizeof(expr));

            if (!parse_expr(p, r_expr, 0)) {
                //free right expression
                free(r_expr); free(index_expr);
                return throw(p->fe, &p->x.s, "expected expression");
            }

            index_expr->right=r_expr;

            if (!parse_next_eq(p, t_ridx)) {
                throw(p->fe, &p->x.s, "expected right bracket to end index");
                note(p->fe, l_idx, "other bracket here");
                return 0;
            }

            parse_set_left(x_expr, left_expr, index_expr);
        } else {
            //not call or index, simple access
            parse_set_left(x_expr, left_access, target);
        }
    } else {
        return -1; //no expression at all
    }

    return 1;
};

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

    //parse ops
    x_expr->op = op_none;

    while(1) {
        op x_op;

        unsigned x_op_prec;

        token* x = parse_peek(p, 1);
        if (!x) return 1; //no more tokens

        switch (x->tt) {
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

            case t_add: {
                x_op = op_add;
                x_op_prec = 10;
                break;
            }

            case t_sub: {
                x_op = op_sub;
                x_op_prec = 10;
                break;
            }

            case t_div: {
                x_op = op_div;
                x_op_prec = 20;
                break;
            }

            case t_mul: {
                x_op = op_mul;
                x_op_prec = 20;
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

        if (!parse_expr(p, r_expr, x_op_prec)) {
            if (left) free(r_expr);
            throw(p->fe, &p->x.s, "expected expression after operator");
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
    switch (e->left) {
        case left_num: print_num((num*)e->left_ref); break;
        case left_expr: printf("("); print_expr((expr*)e->left_ref); printf(")"); break;
        case left_access: {
            printf("%s", (char*)e->left_ref);
            break;
        }
        case left_call: {
            printf("%s()", (char*)e->left_ref);
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

//int parse_top(frontend* fe, top* v) {
//    type_id x;
//    if (!parse_type_id(fe, &x)) return 0;
//    v->ty = x;
//
//    if (!parse_id(fe)) return throw(fe, NULL, "expected identifier");
//    v->name = fe->pos;
//
//    if (!parse_skip(fe)) return throw(fe, NULL, "expected function or item definition");
//
//    if (fe->x == '(') {
//        v->func = 1;
//        v->args = vector_new(sizeof(arg));
//
//        //similar to parse_expr's fn call handling
//        while (parse_skip(fe), fe->x != ')') {
//            //no paren, go back
//            backtrack(fe, 1);
//
//            type_id arg_t;
//
//            if (!parse_type_id(fe, &arg_t)) return throw(fe, NULL, "expected argument type");
//            if (!parse_id(fe)) return throw(fe, NULL, "expected argument identifier");
//
//            arg arg = {.name=fe->pos, .ty=arg_t};
//            vector_push(&v->args, &arg);
//
//            //consume comma, if exists
//            parse_skip(fe);
//            if (fe->x != ',') {
//                backtrack(fe, 1);
//            }
//        }
//    } else if (fe->x != '=') {
//        return throw(fe, NULL, "expected ( or =");
//    }
//
//    parse_skip(fe);
//    if (fe->x == '{') {
//        parse_skip(fe);
//
//    } else {
//
//    }
//
//    map* scope = vector_get(&fe->scope, fe->scope.length-1);
//
//    char* name_s = spanstr(&v->name);
//    map_insert(scope, name_s, &v);
//    free(name_s);
//
//    return 1;
//}

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