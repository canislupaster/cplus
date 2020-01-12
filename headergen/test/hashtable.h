// Automatically generated header.

#include <stdint.h>
#include <string.h>
#define CONTROL_BYTES 16
typedef struct {
	uint8_t control_bytes[CONTROL_BYTES];
} bucket;
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
typedef struct {
	map* map;

	char c;
	unsigned long bucket;

	void* key;
	void* x;
	char current_c;
	bucket* bucket_ref;
} map_iterator;
typedef struct {
	void* val;
	char exists;
} map_insert_result;
map map_new();
void map_configure_string_key(map* map, unsigned long size);
void map_configure_ulong_key(map* map, unsigned long size);
void map_configure_ptr_key(map* map, unsigned long size);
map_iterator map_iterate(map* map);
int map_next(map_iterator* iterator);
void* map_find(map* map, void* key);
map_insert_result map_insertcpy(map* map, void* key, void* v);
int map_remove(map* map, void* key);
void map_free(map* map);
