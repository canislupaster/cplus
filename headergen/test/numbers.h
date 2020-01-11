// Automatically generated header.

#include <stdint.h>
#include <math.h>
typedef struct {
	enum {
		num_decimal,
		num_integer,
	} ty;

	union {
		uint64_t uint;
		int64_t integer;
		long double decimal;
	};
} num;
extern ;
num ONE;
