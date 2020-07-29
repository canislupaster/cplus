// Automatically generated header.

#pragma once
extern int CALL_COST;
#include "expr.h"
int cost(expr* exp);
typedef struct {
	map_t usages;
} optimizer;
void reduce(expr** exp);
