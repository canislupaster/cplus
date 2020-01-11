// Automatically generated header.

#include <string.h>
#include <stdint.h>
#include "siphash.h"
#include "siphash.h"
#include "util.h"
#include "util.h"
#include "util.h"
#define CONTROL_BYTES 16
#define DEFAULT_BUCKETS 2
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
extern {0};

static struct {
	char initialized;
	int key[4]; //uint64_t with ints
}
	SIPHASH_KEY;
typedef struct {
	map* map;

	void* key;
	uint64_t h1;
	uint8_t h2;

	unsigned long probes;

	bucket* current;
	/// temporary storage for c when matching
	char c;
	} map_probe_iterator;
;

static uint64_t make_h1(uint64_t hash);
extern uint8_t MAP_SENTINEL_H2;
;

static uint8_t make_h2(uint64_t hash);
uint64_t hash_string(char** x);
uint64_t hash_ulong(unsigned long* x);
int compare_string(char** left, char** right);
int compare_ulong(unsigned long* left, unsigned long* right);
long map_bucket_size(map* map);
map map_new();
void map_configure(map* map, unsigned long size);
void map_configure_string_key(map* map, unsigned long size);
void map_configure_ulong_key(map* map, unsigned long size);
void map_configure_ptr_key(map* map, unsigned long size);
int map_load_factor(map* map);
uint16_t mask(bucket* bucket, uint8_t h2);
map_iterator map_iterate(map* map);
int map_next(map_iterator* iterator);
static map_probe_iterator map_probe_hashed(map* map, void* key, uint64_t h1, uint8_t h2);
static map_probe_iterator map_probe(map* map, void* key);
static int map_probe_next(map_probe_iterator* probe_iter);
static uint16_t map_probe_empty(map_probe_iterator* probe_iter);
extern uint16_t MAP_PROBE_EMPTY;
;

static void* map_probe_match(map_probe_iterator* probe_iter);
void* map_find(map* map, void* key);
typedef struct {
	char* pos;
	/// 1 if already existent
	char exists;
} map_probe_insert_result;
;

static map_probe_insert_result map_probe_insert(map_probe_iterator* probe);
static void* map_probe_remove(map_probe_iterator* probe);
void map_resize(map* map);
map_insert_result map_insert(map* map, void* key);
map_insert_result map_insertcpy(map* map, void* key, void* v);
int map_remove(map* map, void* key);
void map_free(map* map);
