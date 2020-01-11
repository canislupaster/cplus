/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
typedef struct {
	unsigned long size;

	unsigned long length;
	char* data;
}vector;
void vector_free(vector *vec);
void drop(void *ptr);
void vector_clear(vector *vec);
void vector_cpy(vector *from,vector *to);
typedef struct {
	vector* vec;

	unsigned long i;
	char rev;
	void* x;
}vector_iterator;
int vector_next(vector_iterator *iter);
vector_iterator vector_iterate(vector *vec);
void *vector_setcpy(vector *vec,unsigned long i,void *x);
void *vector_set(vector *vec,unsigned long i);
void *vector_insertcpy(vector *vec,unsigned long i,void *x);
void *vector_insert(vector *vec,unsigned long i);
void *vector_get(vector *vec,unsigned long i);
int vector_remove(vector *vec,unsigned long i);
void *heapcpy(size_t size,const void *val);
void *vector_popcpy(vector *vec);
int vector_pop(vector *vec);
void *vector_pushcpy(vector *vec,void *x);
void *resize(void *ptr,size_t size);
void *heap(size_t size);
void *vector_push(vector *vec);
void vector_init(vector *vec,unsigned long length);
vector vector_new(unsigned long size);
