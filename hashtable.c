#include "siphash.c"

#include "string.h"
#include "stdint.h"
#include "emmintrin.h"

#define CONTROL_BYTES 16
#define DEFAULT_BUCKETS 2

const char DEFAULT_BUCKET[CONTROL_BYTES] = {0};

struct {
    char initialized;
    int key[4]; //uint64_t with ints
}
    SIPHASH_KEY = {.initialized=0, .key=0};

typedef struct {
    uint8_t control_bytes[CONTROL_BYTES];
} bucket;

typedef struct {
    unsigned long key_size;
    unsigned long size;
    /// hash and compare
    uint64_t (*hash)(void*);
    /// compare(&left, &right)
    int (*compare)(void*, void*);

    unsigned long length;
    unsigned long num_buckets;
    char* buckets;
} map;

typedef struct {
    map* map;

    char c;
    unsigned long bucket;

    void* current;
    char current_c;
    bucket* bucket_ref;
} map_iterator;

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

static uint64_t make_h1(uint64_t hash) {
    return (hash << 7) >> 7;
}

const uint8_t MAP_SENTINEL_H2 = 0x80;

static uint8_t make_h2(uint64_t hash) {
    uint8_t h2 = (hash >> 57);
    if (h2 == 0) h2 = 0xFFu; //if h2 is zero, its control byte will be marked empty, so just invert it

    return h2;
}

/// uses siphash
uint64_t hash_string(char* x) {
    return siphash24((uint8_t*)x, strlen(x), (char*)SIPHASH_KEY.key);
}

int compare_string(char* left, char* right) {
    return strcmp(left, right);
}

unsigned long map_bucket_size(map* map) {
    return CONTROL_BYTES + CONTROL_BYTES*map->size;
}

map map_new() {
    map map = {.length=0, .num_buckets=DEFAULT_BUCKETS};

    return map;
}

void map_configure(map* map, unsigned long size) {
    //set siphash key
    if (!SIPHASH_KEY.initialized) {
        SIPHASH_KEY.initialized = 1;

        for (char i=0;i<4;i++) {
            SIPHASH_KEY.key[i] = rand();
        }
    }

    map->size = size+map->key_size;

    unsigned long x = DEFAULT_BUCKETS*map_bucket_size(map);
    map->buckets = malloc(x);

    for (unsigned long i=0; i<map->num_buckets; i++) {
        memcpy(map->buckets + i*map_bucket_size(map), DEFAULT_BUCKET, CONTROL_BYTES);
    }
}

void map_configure_string_key(map* map, unsigned long size) {
    map->key_size = sizeof(char*); //string reference is default key

    map->hash = (uint64_t(*)(void*))hash_string;
    map->compare = (int(*)(void*, void*))compare_string;

    map_configure(map, size);
}

int map_load_factor(map* map) {
    return ((double)(map->length) / (double)(map->num_buckets*CONTROL_BYTES)) > 0.2;
}

uint16_t mask(bucket* bucket, uint8_t h2) {
    __m128i control_byte_vec = _mm_loadu_si64(bucket->control_bytes);

    __m128i result = _mm_cmpeq_epi8(_mm_set1_epi8(h2), control_byte_vec);
    return _mm_movemask_epi8(result);
}

map_iterator map_iterate(map* map) {
    map_iterator iterator = {
        map,
        .bucket=0, .c=0,
        .bucket_ref=NULL, .current=NULL
    };

    return iterator;
}

//todo: sse
int map_next(map_iterator* iterator) {
    //while bucket is less than the last bucket in memory
    while (iterator->bucket < iterator->map->num_buckets) {
        iterator->bucket_ref = (bucket*)(iterator->map->buckets + map_bucket_size(iterator->map)*iterator->bucket);

        //if filled, update current
        unsigned char filled = iterator->bucket_ref->control_bytes[iterator->c] != 0
                && iterator->bucket_ref->control_bytes[iterator->c] != MAP_SENTINEL_H2;

        if (filled) {
            iterator->current_c = iterator->c;
            iterator->current = (char*)iterator->bucket_ref + CONTROL_BYTES + (iterator->map->size * iterator->c);
        }

        //increment byte or bucket
        if (++iterator->c >= CONTROL_BYTES) {
            iterator->bucket++;
            iterator->c = 0;
        }

        //if filled (we've already updated current, return
        if (filled) {
            return 1;
        }
    }

    return 0;
}

map_probe_iterator map_probe_hashed(map* map, void* key, uint64_t h1, uint8_t h2) {
    map_probe_iterator probe_iter = {
            map,
            key, h1, h2,
            .probes=0, .current=NULL
    };

    return probe_iter;
}

map_probe_iterator map_probe(map* map, void* key) {
    uint64_t hash = map->hash(key);
    return map_probe_hashed(map, key, make_h1(hash), make_h2(hash));
}

int map_probe_next(map_probe_iterator* probe_iter) {
    if (probe_iter->probes >= probe_iter->map->num_buckets) return 0;

    uint64_t idx =
            (probe_iter->h1
            + (uint64_t)((0.5*probe_iter->probes) + (0.5*probe_iter->probes*probe_iter->probes)))

            % probe_iter->map->num_buckets;

    probe_iter->current = (bucket*)(probe_iter->map->buckets + idx*map_bucket_size(probe_iter->map));
    probe_iter->probes++;

    return 1;
}

uint16_t map_probe_empty(map_probe_iterator* probe_iter) {
    return mask(probe_iter->current, 0);
}

const uint16_t MAP_PROBE_EMPTY = UINT16_MAX;

void* map_probe_match(map_probe_iterator* probe_iter) {
    uint16_t masked = mask(probe_iter->current, probe_iter->h2);

    //there is a matched byte, find which one
    if (masked > 0) {
        for (probe_iter->c=0; probe_iter->c<CONTROL_BYTES; probe_iter->c++) {
            if ((masked >> probe_iter->c) & 0x01) {
                void* compare_key =
                        (char*)probe_iter->current + CONTROL_BYTES + (probe_iter->map->size * probe_iter->c);

                if (probe_iter->map->compare(probe_iter->key, compare_key)==0) return compare_key;
            }
        }
    }

    return NULL;
}

/// returns ptr to key alongside value
void* map_find(map* map, void* key) {
    map_probe_iterator probe = map_probe(map, key);
    while (map_probe_next(&probe) && map_probe_empty(&probe) != MAP_PROBE_EMPTY) {
        void* x = map_probe_match(&probe);
        if (x) return x;
    }

    return NULL;
}

char* map_probe_insert(map_probe_iterator* probe) {
    unsigned char c;
    bucket* bucket_ref=NULL;

    while (map_probe_next(probe)) {
        //already exists, cannot insert
        if (map_probe_match(probe)) return NULL;

        uint16_t empty = map_probe_empty(probe);

        //look for empty slot and set bucket
        //dont use sse for flexibility to check for sentinels and stuff
        if (!bucket_ref) {
            for (unsigned char c2=0; c2<CONTROL_BYTES; c2++) {
                //empty or sentinel
                if (probe->current->control_bytes[c2] == 0
                || probe->current->control_bytes[c2] == MAP_SENTINEL_H2) {

                    bucket_ref=probe->current; c=c2;
                    break;
                }
            }
        }

        //empty bucket, stop probing
        if (empty == MAP_PROBE_EMPTY) break;
    }

    if (!bucket_ref) return NULL;

    //set h2
    bucket_ref->control_bytes[c] = probe->h2;
    //return insertion point
    return (char*)bucket_ref + CONTROL_BYTES + (probe->map->size * c);
}

//returns the item which can be used/copied if the hashmap is not being used in parallel
void* map_probe_remove(map_probe_iterator* probe) {
    while (map_probe_next(probe) && map_probe_empty(probe) != MAP_PROBE_EMPTY) {
        void* x = map_probe_match(probe);
        //found, set h2 to sentinel
        if (x) {
            probe->current->control_bytes[probe->c] = MAP_SENTINEL_H2;
            return x;
        }
    }

    return NULL;
}

void map_resize(map* map) {
    while (map_load_factor(map)) {
        //double
        unsigned long old_num_buckets = map->num_buckets;
        map->num_buckets = map->num_buckets*2;

        map->buckets = realloc(map->buckets, map->num_buckets*map_bucket_size(map));

        for (unsigned long i=old_num_buckets; i<map->num_buckets; i++) {
            memcpy(map->buckets + i*map_bucket_size(map), DEFAULT_BUCKET, CONTROL_BYTES);
        }

        //rehash
        map_iterator iter = map_iterate(map);
        while (map_next(&iter) && iter.bucket < old_num_buckets) {
            uint64_t hash = map->hash(iter.current);
            uint64_t h1 = make_h1(hash);

            //if it has moved buckets, remove and insert into new bucket
            if (h1 % map->num_buckets != iter.bucket) {
                iter.bucket_ref->control_bytes[iter.current_c] = 0x80;

                map_probe_iterator probe = map_probe_hashed(map, iter.current, h1, make_h2(hash));
                //copy things over
                void* insertion = map_probe_insert(&probe);
                memcpy(insertion, iter.current, map->size);
            }
        }
    }
}

/// one if success
int map_insert(map* map, void* key, void* v) {
    map_probe_iterator probe = map_probe(map, key);

    char* pos = map_probe_insert(&probe);
    if (!pos) return 0; //no bucket found

    //store key
    memcpy(pos, key, map->key_size);
    //store value
    memcpy(pos + map->key_size, v, map->size - map->key_size);

    map->length++;

    map_resize(map);
    return 1;
}

int map_remove(map* map, void* key) {
    map_probe_iterator probe = map_probe(map, key);

    map_probe_remove(&probe);

    map->length--;

    map_resize(map);
    return 1;
}

void map_free(map* map) {
    free(map->buckets);
}