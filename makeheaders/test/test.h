/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

int do_thing();

struct span;
struct module;
struct span {
	struct module* mod;

	char* start;
	char* end;
};
typedef struct span span;
struct module {
	char* name;

	span s;
};

int mainthing();
