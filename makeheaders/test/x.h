/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
struct span;
struct module;
struct span {
	struct module* mod;

	char* start;
	char* end;
};

int do_thing();

typedef struct span span;
struct module {
	char* name;

	span s;
};
