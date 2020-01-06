#include "lexer.h"

int lex_next(lexer* l) {
	l->pos.end++;
	if (!lex_eof(l)) {
		l->x = *(l->pos.end - 1);
		return 1;
	} else {
		return 0;
	}
}

void lex_back(lexer* l) {
	l->pos.end--;
}

int lex_eof(lexer* l) {
	return l->pos.end > l->mod->s.end;
}

/// marks current char as start
void lex_mark(lexer* l) {
	l->pos.start = l->pos.end - 1;
}

/// returns null when eof
char lex_peek(lexer* l) {
	if (l->pos.end - 1 >= l->mod->s.end)
		return 0;
	return *(l->pos.end);
}

/// does not consume if unequal
int lex_next_eq(lexer* l, char x) {
	if (lex_peek(l) == x) {
		lex_next(l);
		return 1;
	} else {
		return 0;
	}
}

/// utility fn
token* token_push(lexer* l, token_type tt) {
	token* t = vector_pushcpy(&l->mod->tokens,
														&(token) {.tt=tt, .s=l->pos});
	return t;
}

num* num_new(num x) {
	return heapcpy(sizeof(num), &x);
}

const char* SKIP = " \n\r/(),.+-=\"";
name ADD_NAME = {.qualifier=NULL, .x="+"};
name SUB_NAME = {.qualifier=NULL, .x="-"};
name EQ_NAME = {.qualifier=NULL, .x="="};

// state is previous state, removed from reserved
void lex_name(lexer* l, char state) {
	name n;
	n.qualifier = NULL;

	while ((l->x = lex_peek(l)) && (l->x == state || strchr(SKIP, l->x) == NULL)) {
		if (l->x == '.') {
			n.qualifier = spanstr(&l->pos);
			lex_mark(l);
		}

		lex_next(l);
	}

	if (!n.qualifier && span_eq(l->pos, "for")) {
		token_push(l, t_for);
	} else {
		if (span_len(&l->pos) == 0) {
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
			if (lex_peek(l) != ' ' && lex_peek(l) != '\t')
				token_push(l, t_sync);
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
				lex_back(l);
				lex_name(l, '/');
				break;
			}
		}

		case '.': {
			if (lex_next_eq(l, '.') && lex_next_eq(l, '.')) { //try consume two more dots
				token_push(l, t_ellipsis);
				break;
			} else {
				lex_back(l);
				lex_name(l, '.');
				break;
			}
		}

		case '(': token_push(l, t_lparen);
			break;
		case ')': token_push(l, t_rparen);
			break;

		case ',':token_push(l, t_comma);
			break;

			//parse string
		case '\"': {
			span str_data = {.start=l->pos.end};
			char escaped = 0;

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
					escaped = 1;
				} else {
					escaped = 0;
				}
			}

			token_push(l, t_str)->val.str = spanstr(&str_data);
			break;
		}

		case '0' ... '9': {
			num number;

			unsigned long decimal_place = 0;
			number.ty = num_integer;
			number.uint = 0;

			do {
				if (l->x >= '0' && l->x <= '9') {
					int val = l->x - '0';
					//modify decimal or uint
					if (number.ty == num_decimal) {
						number.decimal += (long double) val / (10 ^ decimal_place);
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
					number.decimal = (long double) number.uint;
					decimal_place = 0;
				} else {
					lex_back(l); //undo consuming non-number char
					break;
				}
			} while (lex_next(l));

			if (number.ty == num_integer) {
				//convert uint to integer
				number.integer = (int64_t) number.uint;

				if ((uint64_t) number.integer < number.uint) {
					throw(&l->pos, "integer overflow");
				}
			}

			token_push(l, t_num)->val.num = num_new(number);

			break;
		}

		case '+':
			if (lex_peek(l) != '+')
				token_push(l, t_add)->val.name = &ADD_NAME;
			else {
				lex_back(l);
				lex_name(l, '+');
			}
			break;
		case '-':
			if (lex_peek(l) != '-')
				token_push(l, t_sub)->val.name = &SUB_NAME;
			else {
				lex_back(l);
				lex_name(l, '-');
			}
			break;
		case '=':
			if (lex_peek(l) != '=')
				token_push(l, t_eq)->val.name = &EQ_NAME;
			else {
				lex_back(l);
				lex_name(l, '=');
			}
			break;

		default: lex_name(l, 0);
	}

	return 1;
}

void lex(frontend* fe) {
	lexer l = {.mod=&fe->current,
			.pos={.start=fe->current->s.start, .end=fe->current->s.start}};

	while (lex_char(&l));
}

void token_free(token* t) {
	switch (t->tt) {
		case t_name: {
			if (t->val.name->qualifier)
				drop(t->val.name->qualifier);
			drop(t->val.name->x);
		}
		case t_num: drop(t->val.num);
			break;

		default:;
	}
}