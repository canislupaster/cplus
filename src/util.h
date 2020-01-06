/* This file was automatically generated.  Do not edit! */
#undef INTERFACE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"

typedef struct {
	unsigned long key_size;
	unsigned long size;

	/// hash and compare
	uint64_t (* hash)(void*);

	/// compare(&left, &right)
	int (* compare)(void*, void*);

	unsigned long length;
	unsigned long num_buckets;
	char* buckets;
} map;

void map_free(map* map);

#define CONTROL_BYTES 16
typedef struct {
	uint8_t control_bytes[CONTROL_BYTES];
} bucket;
typedef struct {
	map* map;

	char c;
	unsigned long bucket;

	void* key;
	void* x;
	char current_c;
	bucket* bucket_ref;
} map_iterator;

int map_next(map_iterator* iterator);

map_iterator map_iterate(map* map);

void memcheck();

int map_remove(map* map, void* key);

void drop(void* ptr);

void* resize(void* ptr, size_t size);

void* heapcpy(size_t size, const void* val);

typedef struct {
	void* val;
	char exists;
} map_insert_result;

map_insert_result map_insertcpy(map* map, void* key, void* v);

#define TRACE_SIZE 10
typedef struct {
	void* stack[TRACE_SIZE];
} trace;

void print_trace(trace* trace);

void* heap(size_t size);

void map_configure_ptr_key(map* map, unsigned long size);

map map_new();

void memcheck_init();

trace stacktrace();
