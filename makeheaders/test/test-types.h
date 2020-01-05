typedef struct span {
	struct module* mod;

	char* start;
	char* end;
} span;

typedef struct module {
	char* name;

	span s;
} module;