// Automatically generated header.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "hashtable.h"
#include "str.h"
#include "hashtable.h"
#define TRACE_SIZE 10
typedef struct {
	void* stack[TRACE_SIZE];
} trace;
extern ;

static struct {
	map alloc_map;
	int initialized;
}
		ALLOCATIONS;
;

trace stacktrace();
void print_trace(trace* trace);
void* heap(size_t size);
void* heapcpy(size_t size, const void* val);
char* heapstr(const char* fmt, ...);
void* resize(void* ptr, size_t size);
void drop(void* ptr);
