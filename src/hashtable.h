/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void drop(void* ptr);
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "math.h"
#include "string.h"
void map_free(map *map);

int map_remove(map* map, void* key);

void* heapcpy(size_t size, const void* val);

void map_cpy(map* from, map* to);
map_insert_result map_insertcpy(map *map,void *key,void *v);

map_insert_result map_insert(map* map, void* key);

void* resize(void* ptr, size_t size);

void map_resize(map* map);
void *map_find(map *map,void *key);
extern const uint16_t MAP_PROBE_EMPTY;
int map_next(map_iterator *iterator);
map_iterator map_iterate(map *map);
#if BUILD_DEBUG
uint16_t mask(bucket *bucket,uint8_t h2);
#endif

int map_load_factor(map* map);

void map_configure_ptr_key(map* map, unsigned long size);

void map_configure_ulong_key(map* map, unsigned long size);

void map_configure_string_key(map* map, unsigned long size);

void* heap(size_t size);

void map_configure(map* map, unsigned long size);
#define DEFAULT_BUCKETS 2
map map_new();

unsigned long map_bucket_size(map* map);

int compare_ptr(void** left, void** right);

int compare_ulong(unsigned long* left, unsigned long* right);

int compare_string(char** left, char** right);

uint64_t hash_ptr(void** x);

uint64_t hash_ulong(unsigned long* x);
uint64_t siphash24(const void *src,unsigned long src_sz,const char key[16]);
uint64_t hash_string(char **x);
extern const uint8_t MAP_SENTINEL_H2;
#define CONTROL_BYTES 16
extern const char DEFAULT_BUCKET[CONTROL_BYTES];
