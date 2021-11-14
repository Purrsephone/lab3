/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 */

#ifndef _BP_H_
#define _BP_H_
#include <inttypes.h>

typedef struct 
{
    uint64_t tag;
    int valid;
    int cond;
    uint64_t target;
} entry_t;

typedef struct 
{
    entry_t entries[1024];
} btb_t;

typedef struct 
{
    int ghr;
    // pht can just be an array of ints where 0 = 00, 1 = 01, 2 = 10, 3 = 11
    int pht[256];
} gshare_t;

typedef struct
{
    gshare_t gshare;
    btb_t btb;
} bp_t;

extern bp_t bp;

void bp_predict(/*......*/);
void bp_update(/*......*/);

#endif
