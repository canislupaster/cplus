#include "vector.h"

vector vector_new(unsigned long size) {
	vector vec = {size, .length=0, .data=NULL};

	return vec;
}

/// returns ptr to insertion point
void* vector_push(vector* vec) {
	vec->length++;

	//allocate or reallocate
	if (!vec->data)
		vec->data = malloc(vec->size);
	else
		vec->data = realloc(vec->data, vec->size * vec->length);

	return vec->data + (vec->length - 1) * vec->size;
}

void* vector_pushcpy(vector* vec, void* x) {
	void* pos = vector_push(vec);
	memcpy(pos, x, vec->size);
	return pos;
}

int vector_pop(vector* vec) {
	if (vec->length == 0)
		return 0;

	vec->length--;
	vec->data = realloc(vec->data, vec->size * vec->length);

	return 1;
}

void* vector_popcpy(vector* vec) {
	if (vec->length == 0)
		return NULL;

	void* x = malloc(vec->size);
	memcpy(x, vec->data + vec->size * vec->length - 1, vec->size);

	vec->length--;
	vec->data = realloc(vec->data, vec->size * vec->length);

	return x;
}

/// returns 1 if removed successfully
int vector_remove(vector* vec, unsigned long i) {
	if (i == vec->length - 1) {
		vec->length--;

		vec->data = realloc(vec->data, vec->size * vec->length);
	}

	//sanity checks
	if (!vec->data || i >= vec->length)
		return 0;

	memcpy(vec->data + i * vec->size,
				 vec->data + (i + 1) * vec->size,
				 (vec->length - 1 - i) * vec->size);
	return 1;
}

void* vector_get(vector* vec, unsigned long i) {
	if (i >= vec->length) {
		return NULL;
	}

	return vec->data + i * vec->size;
}

void* vector_set(vector* vec, unsigned long i) {
	if (i >= vec->length) {
		vec->length = i + 1;
		vec->data = realloc(vec->data, vec->size * vec->length);
	}

	return vec->data + i * vec->size;
}

void* vector_setcpy(vector* vec, unsigned long i, void* x) {
	void* pos = vector_set(vec, i);
	memcpy(pos, x, vec->size);

	return pos;
}

vector_iterator vector_iterate(vector* vec) {
	vector_iterator iter = {
			vec, .i=0, .rev=0
	};

	return iter;
}

int vector_next(vector_iterator* iter) {
	iter->x = iter->vec->data + (iter->rev ? iter->vec->length - 1 - iter->i : iter->i) * iter->vec->size;
	iter->i++;

	if (iter->i > iter->vec->length)
		return 0;
	else
		return 1;
}

void vector_cpy(vector* from, vector* to) {
	*to = *from;

	to->data = malloc(from->size * from->length);
	memcpy(to->data, from->data, from->size * from->length);
}

void vector_clear(vector* vec) {
	if (vec->data) {
		free(vec->data);
		vec->data = NULL;
	}

	vec->length = 0;
}

void vector_free(vector* vec) {
	if (vec->data)
		free(vec->data);
}