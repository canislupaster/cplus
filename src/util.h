/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdarg.h>

#define TRACE_SIZE 10

typedef struct {
	void* stack[TRACE_SIZE];
} trace;

void memcheck();
void *resize(void *ptr,size_t size);
int vasprintf(char **strp,const char *fmt,va_list ap);
char *heapstr(const char *fmt,...);
void *heapcpy(size_t size,const void *val);
void *heap(size_t size);
void memcheck_init();
void drop(void *ptr);
void print_trace(trace *trace);
trace stacktrace();
