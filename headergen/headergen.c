#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define MIN(a, b) (a < b ? a : b)

#include "../src/hashtable.h"
#include "../src/vector.h"

typedef struct {
	struct file* file;
	int objectid; //preserved for order
	char* declaration;
	vector deps; //char*

	int pub;
} object;

typedef struct file {
	map objects;
	vector deps; // all deps
	vector includes;
	vector blind_includes;

	int pub; //has any published objects
} file;

typedef struct {
	int objectid;
	int braces;

	char* path;
	char* file;
	vector tokens;

	file* current;
	vector* deps;

	map files;
} state;

object* object_new(state* state_) {
	object* obj = malloc(sizeof(object));
	obj->objectid = state_->objectid;
	state_->objectid++;

	obj->file = state_->current;
	obj->deps = vector_new(sizeof(char*));
	obj->pub = 0;

	state_->deps = &obj->deps;

	return obj;
}

typedef struct {
	char* start;
	int len;
} token;

char* SKIP = "*=<>/-+&[]();,.";
//braindump
char* SKIP_WORDS[] = {"extern", "if", "else", "return"};

int ws(state* state) {
	return *state->file == ' ' || *state->file == '\r' || *state->file == '\n' || *state->file == '\t';
}

void parse_ws(state* state) {
	while (ws(state)) state->file++;
}

void parse_define(state* state) {
	while (*state->file != '\n') {
		if (*state->file == '\\') state->file++;
		state->file++;
	}
}

void parse_skip(state* state) {
	parse_ws(state);

	//skip strings
	if (*state->file == '\"') {
		state->file++; //skip "
		while (*state->file != '\"' && *state->file) {
			if (*state->file == '\\') state->file++;
			state->file++;
		}

		state->file++; //skip "

		return parse_skip(state);
	}

	if (*state->file == '\'') {
		state->file++; //skip '
		if (*state->file == '\\') state->file++;
		state->file++; //skip char
		state->file++; //skip '

		return parse_skip(state);
	}

	//skip reserved tokens or numbers
	if (*state->file && (strchr("0123456789", *state->file) || strchr(SKIP, *state->file))) {
		state->file++;
		return parse_skip(state);
	}

	for (int i = 0; i < sizeof(SKIP_WORDS) / sizeof(char*); i++) {
		if (strncmp(SKIP_WORDS[i], state->file, strlen(SKIP_WORDS[i])) == 0) {
			char* start = state->file;
			state->file += strlen(SKIP_WORDS[i]);

			if (ws(state) || !*state->file
					|| strchr(SKIP, *state->file)
					|| strchr("{}", *state->file))
				return parse_skip(state); //parse skips after word
			else state->file = start; //no skip or brace or ws after word, not parsed
		}
	}
}

int parse_peek_brace(state* state) {
	parse_skip(state);
	if (!*state->file) return 0;

	if (*state->file == '{' && state->braces == 0) return 1;
	else return 0;
}

int parse_start_brace(state* state) {
	parse_skip(state);
	if (!*state->file) return 0;

	if (*state->file == '{' && state->braces == 0) {
		state->file++;
		return 1;
	} else return 0;
}

int parse_end_brace(state* state) {
	parse_skip(state);
	if (!*state->file) return 1;

	if (*state->file == '}' && state->braces == 0) {
		state->file++;
		return 1;
	} else return 0;
}

int parse_sep(state* state) {
	parse_ws(state);
	if (!*state->file) return 0;

	if (strchr(";[(=*,", *state->file)) return 1;
	else return 0;
}

token* parse_string(state* state) {
	parse_ws(state);

	if (*state->file == '\"') {
		token* tok = vector_push(&state->tokens);

		state->file++;
		tok->start = state->file;
		tok->len = 0;

		while (*state->file != '\"' && *state->file) {
			if (*state->file == '\\') {
				state->file++;
				tok->len++;
			}

			state->file++;
			tok->len++;
		}

		state->file++;
		return tok;
	} else {
		return NULL;
	}
}

int token_eq(token* tok, char* str) {
	return tok->len == strlen(str) && strncmp(tok->start, str, tok->len) == 0;
}

/// parses identifiers
token* parse_token(state* state) {
	parse_skip(state);
	if (!*state->file) return NULL;

	if (*state->file == '{') {
		state->braces++;
		state->file++;
		return parse_token(state);
	}

	if (*state->file == '}') {
		state->braces--;
		state->file++;
		return parse_token(state);
	}

	token tok;
	tok.start = state->file;
	tok.len = 0;

	while (!ws(state) && *state->file
			&& !strchr(SKIP, *state->file)
			&& !strchr("{}", *state->file)) {

		tok.len++;
		state->file++;
	}

	return vector_pushcpy(&state->tokens, &tok);;
}

char* range(char* start, char* end) {
	char* str = malloc(end - start + 1);
	memcpy(str, start, end - start);
	str[end - start] = 0;
	return str;
}

char* token_str(state* state, token* tok) {
	if (tok == NULL) {
		fprintf(stderr, "expected token at %s...",
						range(state->file, state->file + MIN(10, strlen(state->file))));

		exit(1);
	}

	char* str = malloc(tok->len + 1);
	memcpy(str, tok->start, tok->len);
	str[tok->len] = 0;
	return str;
}

char* prefix(char* str, char* prefix) {
	if (*prefix == 0) return str;

	char* new_str = malloc(strlen(str) + strlen(prefix) + 1);
	memcpy(new_str, prefix, strlen(prefix));
	memcpy(new_str + strlen(prefix), str, strlen(str) + 1); //copy null byte
	free(str);

	return new_str;
}

char* affix(char* str, char* affix) {
	if (*affix == 0) return str;

	char* new_str = malloc(strlen(str) + strlen(affix) + 1);
	memcpy(new_str, str, strlen(str) + 1);
	memcpy(new_str + strlen(str), affix, strlen(affix) + 1);
	free(str);

	return new_str;
}

char* dirname(char* filename) {
	char* slash = NULL;

	for (char* fnptr = filename; *fnptr; fnptr++) {
		if (*fnptr == '/')
			slash = fnptr + 1; //include slash
		else if (*fnptr == '\\' && *fnptr + 1 == '\\')
			slash = fnptr + 2;
	}

	if (!slash) return "";

	char* new_path = malloc(slash - filename + 1);
	memcpy(new_path, filename, slash - filename);
	new_path[slash - filename] = 0;

	return new_path;
}

void add_dep(state* state, char* dep) {
	if (state->deps) vector_pushcpy(state->deps, &dep);
	vector_pushcpy(&state->current->deps, &dep);
}

object* parse_object(state* state, int fn) {
	object* obj = NULL;
	char* name = NULL;
	char static_ = 0;

	vector* old_deps = state->deps;

	token* first = parse_token(state);
	if (!first) return NULL;

	if (token_eq(first, "static")) {
		static_ = 1;
		first = parse_token(state);
	}

	char* start = first->start;

	if (token_eq(first, "#include")) {
		parse_ws(state);
		if (state->file[1] == '<') {
			parse_define(state);
			char* str = range(start, state->file);
			vector_pushcpy(&state->current->blind_includes, &str);
		}

		char* path_str = prefix(token_str(state, parse_string(state)), dirname(state->path));
		char* path = realpath(path_str, NULL);
		free(path_str);

		if (!path) {
			char* str = range(start, state->file);
			vector_pushcpy(&state->current->blind_includes, &str);
			return NULL;
		}

		vector_pushcpy(&state->current->includes, &path);
		return NULL;
	} else if (token_eq(first, "#define")) {
		obj = object_new(state);
		name = token_str(state, parse_token(state));

		parse_define(state);

		obj->declaration = affix(range(start, state->file), "\n"); //no semicolon for defines
	} else if (token_eq(first, "typedef")) {
		obj = object_new(state);

		object* inner_obj = parse_object(state, 0); //parse thing inside
		if (inner_obj) obj->deps = inner_obj->deps; //copy deps

		name = token_str(state, parse_token(state));
	} else if (token_eq(first, "enum")) {
		obj = object_new(state);

		name = NULL; //anonymous default
		if (!parse_peek_brace(state))
			name = prefix(token_str(state, parse_token(state)), "enum ");

		if (parse_start_brace(state)) {
			while (!parse_end_brace(state)) {
				char* str = token_str(state, parse_token(state));
				map_insertcpy(&state->current->objects, &str, &obj);
			}
		} else if (name) {
			add_dep(state, name);
			return NULL;
		}

		obj->declaration = affix(range(start, state->file), ";\n");
	} else if (token_eq(first, "struct") || token_eq(first, "union")) {
		char* kind_prefix;
		if (token_eq(first, "struct")) kind_prefix = "struct ";
		else if (token_eq(first, "union")) kind_prefix = "union ";
		else return NULL;

		if (!parse_peek_brace(state)) {
			name = prefix(token_str(state, parse_token(state)), kind_prefix);
		}

		if (parse_start_brace(state)) {
			obj = object_new(state);

			char* ty_prefix = "";

			while (!parse_end_brace(state)) {
				parse_object(state, 0); //parse field type, add deps
				parse_token(state); // parse field name

				parse_ws(state);
			}

			obj->declaration = affix(range(start, state->file), ";\n");
		} else if (name) {
			add_dep(state, name);
			return NULL;
		}
	} else { //function
		if (!fn) {
			char* str = token_str(state, first);
			add_dep(state, str);
			return NULL;
		}

		obj = object_new(state);

		state->file = start;
		parse_object(state, 0);

		name = token_str(state, parse_token(state));

		parse_ws(state);
		state->file++; //skip (

		parse_ws(state);
		while (*state->file != ')') {
			token* tok = parse_token(state);
			if (parse_sep(state)) continue; //name, not type

			char* str = token_str(state, tok);
			add_dep(state, str);

			parse_ws(state);
		}

		state->file++; //skip )

		state->deps = &obj->deps;

		if (parse_start_brace(state)) {
			while (!parse_end_brace(state)) {
				token* tok = parse_token(state);

				if (tok) {
					char* str = token_str(state, tok);
					add_dep(state, str);
				}
			}
		}
	}

	state->deps = old_deps;

	if (obj && name && !static_) {
		map_insertcpy(&state->current->objects, &name, &obj);

		return obj;
	}

	return NULL;
}

char* ext(char* filename) {
	char* dot = "";
	for (; *filename; filename++) {
		if (*filename == '.') dot = filename;
	}

	return dot;
}

void pub_deps(file* file, object* obj) {
	if (obj->pub) return; //already marked
	obj->pub = 1;

	vector_iterator dep_iter = vector_iterate(&obj->deps);
	while (vector_next(&dep_iter)) {
		object** obj_dep = map_find(&file->objects, dep_iter.x);
		if (!obj_dep) continue;

		pub_deps(file, *obj_dep);
	}
}

int main(int argc, char** argv) {
	state state_ = {.files=map_new(), .deps=NULL};
	map_configure_string_key(&state_.files, sizeof(file));

	// PARSE STEP: GO THROUGH ALL FILES AND PARSE OBJECTS N STUFF
	for (int i = 1; i < argc; i++) {
		char* filename = argv[i];
		if (strcmp(ext(filename), ".c") != 0) continue;

		char* path = realpath(filename, NULL);

		if (!path) {
			fprintf(stderr, "%s does not exist", filename);
			free(path);
			continue;
		}

		FILE* handle = fopen(filename, "rb");
		if (!handle) {
			fprintf(stderr, "cannot read %s", filename);
			free(path);
			continue;
		}

		//pretty much copied from frontend
		fseek(handle, 0, SEEK_END);
		unsigned long len = ftell(handle);

		rewind(handle);

		char* str = heap(len + 1);

		fread(str, len, 1, handle);
		str[len] = 0;

		path[strlen(path) - 1] = 'h'; //.c -> .h for finds and gen
		file* new_file = map_insert(&state_.files, &path).val;

		new_file->objects = map_new();
		map_configure_string_key(&new_file->objects, sizeof(object*));

		new_file->pub = 0;
		new_file->deps = vector_new(sizeof(char*));
		new_file->includes = vector_new(sizeof(char*));
		new_file->blind_includes = vector_new(sizeof(char*));

		state_.file = str;
		state_.path = path;

		state_.braces = 0;
		state_.objectid = 0;

		state_.tokens = vector_new(sizeof(token));

		state_.current = new_file;
		state_.deps = NULL;

		while (*state_.file != 0) {
			parse_object(&state_, 1);
		}

		vector_clear(&state_.tokens);

		free(str);
	}

	//LINK STEP: LINK DEPENDENCIES BETWEEN FILES AND THEIR INCLUDE FILES (theoretically, this could be put recursively into the parsing step)
	map_iterator file_iter = map_iterate(&state_.files);
	while (map_next(&file_iter)) {
		file* this = file_iter.x;

		//search for dependencies in file includes
		vector_iterator inc_iter = vector_iterate(&this->includes);

		while (vector_next(&inc_iter)) {
			file* inc_file = map_find(&state_.files, inc_iter.x);
			if (!inc_file) continue;

			vector_iterator dep_iter = vector_iterate(&this->deps);
			while (vector_next(&dep_iter)) {
				object** inc_obj = map_find(&inc_file->objects, dep_iter.x);
				if (!inc_obj) continue;

				inc_file->pub = 1;
				pub_deps(inc_file, *inc_obj);
			}
		}
	}

	//GENERATION: GENERATE ALL HEADERS
	map_iterator filegen_iter = map_iterate(&state_.files);
	while (map_next(&filegen_iter)) {
		file* gen_this = filegen_iter.x;
		if (!gen_this->pub) continue;

		char* filename = *(char**) filegen_iter.key;

		FILE* handle = fopen(filename, "w");
		const char* header_header = "// Automatically generated header.\n\n";
		fwrite(header_header, strlen(header_header), 1, handle);

		vector ordered = vector_new(sizeof(object*)); //order all objects using objectid

		//expand ordered to correct length
		map_iterator pub_iter = map_iterate(&gen_this->objects);
		while (map_next(&pub_iter)) {
			object* pub_obj = *(object**) pub_iter.x;
			if (!pub_obj->pub) continue;

			object* compare_obj;
			//binary search/sort
			double halves = 1;
			double offset = 1;

			while (pow(0.5, halves) >= (1 / (double) (ordered.length + 1))) {
				double pos = pow(0.5, halves) * offset;
				unsigned long compare = (unsigned long) (pos * (double) ordered.length);

				compare_obj = *(object**) vector_get(&ordered, compare);
				if (!compare_obj) break;

				if (pub_obj->objectid > compare_obj->objectid) {
					halves++;

					offset *= 2;
					offset += 1;
				} else {
					halves++;

					offset *= 2;
					offset -= 1;
				}
			}

			double pos = pow(0.5, halves) * offset;
			unsigned long insertion = (unsigned long) floor(pos * (double) (ordered.length + 1));

			vector_insertcpy(&ordered, insertion, &pub_obj);
		}

		vector_iterator blind_iter = vector_iterate(&gen_this->blind_includes);
		while (vector_next(&blind_iter)) {
			char* inc = *(char**) blind_iter.x;
			fwrite(inc, strlen(inc), 1, handle);
		}

		vector_iterator ordered_iter = vector_iterate(&ordered);
		while (vector_next(&ordered_iter)) {
			object* ordered_obj = *(object**) ordered_iter.x;
			fwrite(ordered_obj->declaration, strlen(ordered_obj->declaration), 1, handle);
		}

		fclose(handle);
	}
}