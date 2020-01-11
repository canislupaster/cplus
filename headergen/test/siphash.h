// Automatically generated header.

#include <stdint.h>
#define DOUBLE_ROUND(v0, v1, v2, v3)        \
    HALF_ROUND(v0,v1,v2,v3,13,16);        \
    HALF_ROUND(v2,v1,v0,v3,17,21);        \
    HALF_ROUND(v0,v1,v2,v3,13,16);        \
    HALF_ROUND(v2,v1,v0,v3,17,21);
uint64_t siphash24(const void* src, unsigned long src_sz, const char key[16]);
