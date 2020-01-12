// Automatically generated header.

typedef struct {
	unsigned long size;

	unsigned long length;
	char* data;
} vector;
typedef struct {
	vector* vec;

	unsigned long i;
	char rev;
	void* x;
} vector_iterator;
vector vector_new(unsigned long size);
void* vector_push(vector* vec);
void* vector_pushcpy(vector* vec, void* x);
int vector_pop(vector* vec);
void* vector_get(vector* vec, unsigned long i);
void* vector_setcpy(vector* vec, unsigned long i, void* x);
vector_iterator vector_iterate(vector* vec);
int vector_next(vector_iterator* iter);
void vector_cpy(vector* from, vector* to);
void vector_free(vector* vec);
