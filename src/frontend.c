#include "frontend.h"
#include "colors.h"

frontend* FRONTEND; //global frontend

const span SPAN_NULL = {.start=NULL};

uint64_t hash_name(name* x) {
	uint64_t qhash = x->qualifier ? hash_string(&x->qualifier) : 0;
	uint64_t nhash = hash_string(&x->x);

	return qhash + nhash;
}

int compare_name(name* x1, name* x2) {
	return strcmp(x1->qualifier, x2->qualifier) == 0 && strcmp(x1->x, x2->x) == 0;
}

///upper bound exclusive, returns 1 if equal
int span_eq(span s, char* x) {
	char c;

	while ((c = *x++) && s.start < s.end) {
		if (*s.start != c) {
			return 0;
		}

		s.start++;
	}

	return 1;
}

unsigned long span_len(span* s) {
	return s->end - s->start;
}

char* spanstr(span* s) {
	char* v = heap(span_len(s) + 1);

	memcpy(v, s->start, span_len(s));
	v[span_len(s)] = 0; //set terminator

	return v;
}

void msg(frontend* fe,
				 const span* s,
				 char color1,
				 char color2,
				 const char* template_empty,
				 const char* template,
				 const char* msg) {
	if (s->start == NULL) {
		set_col(stdout, color1);
		printf(template_empty, fe->file);

		set_col(stdout, color2);
		printf("%s\n\n", msg);
	}

	//while the file is probably the same, we may have to reconstruct line and col if custom span
	unsigned long line = 0;
	unsigned long col = 0;

	span line_span = *s;
	//count cols backwards
	while (*line_span.start != '\n' && line_span.start >= fe->s.start) {
		col++; //add column for every time we decrement to find previous linebreak
		line_span.start--;
	}

	line_span.start++;

	//...then count up for lines
	for (char* pos = line_span.start; pos > fe->s.start; pos--) {
		if (*pos == '\n')
			line++;
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
			span* x = vector_pushcpy(&lines,
															 &(span) {.start=line_start, .end=pos});

			line_start = pos + 1; //skip newline
		}
	}

	//lines start at 1
	line++;

	int digits = (int) log10(line + lines.length) + 1;

	set_col(stdout, color1);
	printf(template, fe->file, line, col);

	set_col(stdout, color2);
	printf("%s\n\n", msg);

	//write lines
	for (unsigned long i = 0; i < lines.length; i++) {
		char* line_num = heap(digits + 1);
		line_num[digits] = 0;

		int l_digits = (int) log10(line + i) + 1;
		sprintf(line_num + (digits - l_digits), "%lu", line + i);

		if (digits - l_digits > 0) {
			//fill with whitespace
			for (unsigned ws = 0; ws < digits - l_digits; ws++)
				line_num[ws] = ' ';
		}

		span* x = vector_get(&lines, i);

		char* str = spanstr(x);
		set_col(stdout, color1);
		printf("%s | ", line_num);
		set_col(stdout, color2);
		printf("%s\n", str);
		free(str);
		//check if original span is contained within line, then highlight
		if ((s->end > x->start)
				&& (s->start >= x->start && s->end <= x->end)) {
			//highlight end - line start
			char* buf = heap((s->end - x->start) + 1);
			buf[s->end - x->start] = 0;

			//fill before-highlight with whitespace
			unsigned long long ws;
			for (ws = 0; ws < s->start - x->start; ws++) {
				buf[ws] = ' ';
			}

			for (unsigned long long hi = ws; hi < ws + (s->end - s->start); hi++) {
				buf[hi] = '^';
			}

			set_col(stdout, color1);
			printf("%s | %s\n", line_num, buf);
		}
	}

	printf("\n");
}

///always returns zero for convenience
int throw(const span* s, const char* x) {
	FRONTEND->errored = 1;
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
		case num_decimal: printf("%Lf", n->decimal);
			break;
		case num_integer: printf("%lli", n->integer);
			break;
	}
}

int is_name(token* x) {
	return x->tt == t_name || x->tt == t_add || x->tt == t_sub;
}

///initialize frontend with file
int read_file(frontend* fe, char* filename) {
	FILE* f = fopen(filename, "rb");
	if (!f)
		return 0;

	fseek(f, 0, SEEK_END);
	//allocate length of string
	unsigned long len = ftell(f);
	char* str = heap(len);
	//back to beginning
	rewind(f);

	fread(str, len, 1, f);

	fe->file = filename;

	fe->s.start = str;
	fe->s.end = str + len;

	return 1;
}

void module_init(module* b) {
	b->ids = map_new();
	map_configure_string_key(&b->ids, sizeof(id));
}

frontend make_frontend() {
	frontend fe = {.tokens=vector_new(sizeof(token))};
	module_init(&fe.global);

	FRONTEND = &fe;

	return fe;
}

void value_free(value* val) {
	vector_free(&val->substitutes);
	map_free(&val->substitute_idx);
	expr_free(val->exp);
}

void id_free(id* xid) {
	value_free(&xid->val);
}

void module_free(module* b) {
	map_iterator iter = map_iterate(&b->ids);
	while (map_next(&iter)) {
		id_free(iter.x);
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