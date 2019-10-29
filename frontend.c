#include "term.c"

#include "vector.c"
#include "hashtable.c"

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
    s_expr, s_ret
} stmt_t;

typedef struct {
    stmt_t t;
    span s;
    void* x;
} stmt;

typedef struct {
    vector stmts;
} block;

typedef struct {
    char* file;
    span s;
    unsigned long len;

    vector tokens;

    block global;
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
    msg(fe, s, "error in %s", "error at %s:%lu:%lu: ", x);
    return 0;
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
                        unsigned char x = l.x - '0';

                        vector_push(&num.digits, &x);
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

void free_token(token* t) {
    switch (t->tt) {
        case t_id: free(t->val);
        case t_num: free(t->val);

        default:;
    }
}

typedef struct {
    frontend* fe;
    token x;

    unsigned long pos;
} parser;

int parse_eof(parser* p) {
    return p->pos == p->fe->tokens.length;
}

int parse_next(parser* p) {
    token* x = vector_get(&p->fe->tokens, p->pos);;
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
    enum {ENUM, STRUCT, UNION} kind;

    size_t size;
    vector members;
} parse_type;

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
    char* target;
    vector args;
} fn_call;

/// linked list
typedef struct {
    op op;
    /// assign after op
    char assign;

    left left;
    span left_span;
    void* left_ref;
    //always expr
    void* right;
} expr;

int parse_expr(parser* p, expr* x_expr, unsigned op_prec); //forward decl

typedef enum {
    ty_const = 0x01, ty_ptr = 0x02, ty_arr = 0x04
} type_flags;

typedef struct {
    span s;
    //no ref, types are allowed to be defined out of order
    char* name;
    expr* size_expr;
    /// if zero, it is just an id
    type_flags flags;
} type_id;

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
//parse anything that can be a type attribution, to be resolved later
int parse_type_id(parser* p, type_id* tid) {
    if (!parse_next_eq(p, t_id)) return 0;

    tid->s.start = p->x.s.start;
    tid->flags = 0;

    if (strcmp(p->x.val, "const")==0) {
        tid->flags |= ty_const;

        if (!parse_next_eq(p, t_id)) return throw(p->fe, &p->x.s, "expected type after const");
    }

    tid->name = p->x.val;
    tid->size_expr = parse_idx(p); //parse array size

    if (tid->size_expr) tid->flags |= ty_arr;

    if (parse_next_eq(p, t_mul)) {
        tid->flags |= ty_ptr;
    }

    tid->s.end = p->x.s.end;

    return 1;
}

void parse_set_left(expr* expr,, left l, void* x) {
    expr->left |= l;
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
        if (n->ty == num_decimal && n->digits.length - iter.i == n->decimal_place) {
            printf(".");
        }
        printf("%u", *(unsigned char*)iter.x);
    }
}

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

/// parses bind or set
int parse_bind_fn(parser* p, stmt* s) {
    type_id tid;
    char* name;

    if (parse_type_id(p, &tid) && parse_next_eq(p, t_id)) {
        name = p->x.val; //name of binding

        if (parse_next_eq(p, t_lparen)) {
            //parse fn
            fn_stmt* fn = malloc(sizeof(fn_stmt));
            span fn_paren = p->x.s;

            fn->ty = tid;
            fn->name = name;
            fn->args = vector_new(sizeof(fn_arg));

            if (!parse_next_eq(p, t_rparen)) {
                //parse arguments
                while (1) {
                    fn_arg arg;

                    if (!parse_type_id(p, &arg.ty)) throw(p->fe, &p->x.s, "expected argument type");
                    if (!parse_next_eq(p, t_id)) throw(p->fe, &p->x.s, "expected argument name");

                    arg.name = p->x.val;
                    vector_push(&fn->args, &arg);

                    if (parse_separated(p, t_comma, t_rparen)) break;

                    if (separator(p)) {
                        free(fn);

                        throw(p->fe, &p->x.s, "expected matching paren for function declaration");
                        note(p->fe, &fn_paren, "other paren here");
                        return 0;
                    }
                }
            }

            if (!parse_block(p, &fn->val)) throw(p->fe, &p->x.s, "expected block for function value");

            s->t=s_fn;
            s->x=fn;
            return 1;
        }

        bind_stmt* bind = malloc(sizeof(bind_stmt));
        bind->name = name;
        bind->ty = tid;

        if (parse_next_eq(p, t_set)) {
            if (!parse_block(p, &bind->val)) {
                throw(p->fe, &p->x.s, "expected block/expression in binding");
                bind->uninitialized=1;
            }
        } else {
            bind->uninitialized=1;
            parse_stmt_end(p);
        }

        s->t=s_bind;
        s->x=bind;
        return 1;
    }

    return 0;
}

int parse_stmt(parser* p, block* b) {
    stmt s = {.s={.start=p->x.s.end}};

    parser peek_parser = *p;
    if (parse_bind_fn(&peek_parser, &s)) {
        *p = peek_parser; //update pos

        s.s.end = p->x.s.end;
        vector_push(&b->stmts, &s);

        return 1;
    }

    int ret = parse_next_eq(p, t_return);

    expr* e = malloc(sizeof(expr));
    if (parse_expr(p, e, 0)) {
        if (!parse_stmt_end(p)) ret=1; //if there isn't a semicolon, it shall be qualified as a return statement, which can later be validated

        s.t = ret ? s_ret : s_expr;
        s.x = e;
        s.s.end = p->x.s.end;

        vector_push(&b->stmts, &s);
        return 1;
    }

    block* sub_b = malloc(sizeof(block)); //try parsing block
    if (parse_block_braced(p, sub_b)) {
        s.t = ret ? s_ret_block : s_block;
        s.x = sub_b;
        s.s.end = p->x.s.end;

        vector_push(&b->stmts, &s);

        return 1;
    }

    free(e); return 0;
}

int parse_block_braced(parser* p, block* b) {
    b->stmts = vector_new(sizeof(stmt));

    if (parse_next_eq(p, t_lbrace)) {
        span l_brace = p->x.s;

        while (!parse_next_eq(p, t_rbrace)) {
            if (separator(p)) {
                throw(p->fe, &p->x.s, "expected matching brace for block");
                note(p->fe, &l_brace, "other brace here");
                return 0;
            }

            if (!parse_stmt(p, b)) {
                throw(p->fe, &p->x.s, "expected statement");
            }
        }

        return 1;
    } else {
        return 0;
    }
}

int parse_block(parser* p, block* b) {
    if (parse_block_braced(p, b)) return 1;

    if (parse_next_eq(p, t_sep)) { //empty block, just separator
        return 1;
    } else if (parse_stmt(p, b)) { //one statement block
        return 1;
    } else {
        throw(p->fe, &p->x.s, "expected block/statement");
        return 0;
    }
}

void parse(frontend* fe) {
    parser p = {fe, .pos=0};
    while (!parse_eof(&p) && parse_stmt(&p, &p.fe->global));
}

int parse_left_expr(parser* p, expr* x_expr) {
    x_expr->left=0;

    if (parse_next_eq(p, t_sub)) x_expr->left |= left_neg;
    if (parse_next_eq(p, t_ref)) x_expr->left |= left_ref;
    if (parse_next_eq(p, t_incr)) x_expr->left |= left_incr;
    if (parse_next_eq(p, t_decr)) x_expr->left |= left_decr;

    if (parse_next_eq(p, t_num)) {
        //copy to left
        x_expr->left_span = p->x.s;
        parse_set_left(x_expr, left_num, p->x.val);
    } else if (parse_next_eq(p, t_id)) {
        char* target = p->x.val;
        x_expr->left_span.start = p->x.s.start;

        if (parse_next_eq(p, t_lparen)) {
            span fn_paren = p->x.s; //TODO: parenthesized procedures

            fn_call* f = malloc(sizeof(fn_call));
            f->args = vector_new(sizeof(expr));

            f->target = target;

            //parse arguments
            while (!parse_next_eq(p, t_rparen)) {
                if (separator(p)) {
                    free(f);

                    throw(p->fe, &p->x.s, "expected matching paren for function call");
                    note(p->fe, &fn_paren, "other paren here");
                    return 0;
                }

                expr expr_arg;
                if (!parse_expr(p, &expr_arg, 0)) {
                    free(f); return throw(p->fe, &p->x.s, "expected expression");
                }

                vector_push(&f->args, &expr_arg);

                if (parse_next_eq(p, t_rparen)) break;

                if (!parse_next_eq(p, t_comma)) {
                    throw(p->fe, &p->x.s, "expected comma to separate argument declarations");
                }

                if (parse_next_eq(p, t_rparen)) break; //paren can come before or after arguments
            }

            x_expr->left_span.end = p->x.s.end;
            parse_set_left(x_expr, left_call, f);
        } else {
            expr* r_expr = parse_idx(p);
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

    x_expr->op = op_none;

    //parse ops
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
    else if (e->left & left_call) printf("%s(%lu args)", ((fn_call*)e->left_ref)->target, ((fn_call*)e->left_ref)->args.length);

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
                printf("%s %s = ", bind->ty.name, bind->name);
                if (bind->uninitialized) printf("uninitialized"); else print_block(&bind->val);
                break;
            }

            case s_block: print_block(s->x);
            case s_ret_block: printf("return "); print_block(s->x);

            case s_fn: {
                fn_stmt* fn = s->x;
                printf("fn %s(%lu) -> %s {\n", fn->name, fn->args.length, fn->ty.name);
                print_block(&fn->val);
                printf("}");
            }

            default:;
        }

        printf("\n");
    }
}

typedef struct {
    span name;
    type_id ty;
} arg;

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
    frontend fe = {.tokens=vector_new(sizeof(token)), .global=vector_new(sizeof(stmt))};

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