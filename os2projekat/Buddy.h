#pragma once
#include "BitMapTree.h"

#define BLOCK_SIZE (4096) // in bytes (NOT less then 4B)
#define BLOCK_NUMBER (8)
#define NEXT(X) *(int*)block(X) // reference
#define ISPOW2(X) ((X & (X-1))==0)

#define NEEDS_BIT(N, B)     (((unsigned long)N >> B) > 0)

#define BITS_TO_REPRESENT(N)                            \
        (NEEDS_BIT(N,  0) + NEEDS_BIT(N,  1) + \
         NEEDS_BIT(N,  2) + NEEDS_BIT(N,  3) + \
         NEEDS_BIT(N,  4) + NEEDS_BIT(N,  5) + \
         NEEDS_BIT(N,  6) + NEEDS_BIT(N,  7) + \
         NEEDS_BIT(N,  8) + NEEDS_BIT(N,  9) + \
         NEEDS_BIT(N, 10) + NEEDS_BIT(N, 11) + \
         NEEDS_BIT(N, 12) + NEEDS_BIT(N, 13) + \
         NEEDS_BIT(N, 14) + NEEDS_BIT(N, 15) + \
         NEEDS_BIT(N, 16) + NEEDS_BIT(N, 17) + \
         NEEDS_BIT(N, 18) + NEEDS_BIT(N, 19) + \
         NEEDS_BIT(N, 20) + NEEDS_BIT(N, 21) + \
         NEEDS_BIT(N, 22) + NEEDS_BIT(N, 23) + \
         NEEDS_BIT(N, 24) + NEEDS_BIT(N, 25) + \
         NEEDS_BIT(N, 26) + NEEDS_BIT(N, 25) + \
         NEEDS_BIT(N, 28) + NEEDS_BIT(N, 25) + \
         NEEDS_BIT(N, 30) + NEEDS_BIT(N, 31)   \
        )

#define BUDDY_N (BITS_TO_REPRESENT(BLOCK_NUMBER) - ISPOW2(BLOCK_NUMBER))

/* returns pointer to nth block */
void* block(int n);

/* allocate 2^i continous memory blocks of size = __BUDDY_BLOCK_SIZE */
void* buddy_alloc(int i);

/* deallocates memory pointed by ptr */
int buddy_dealloc(void* ptr);

/* calls bitmapTree_init() and initializes buddy_blocks */
void buddy_init(void * space, int n);

/* prints buddy info */
void buddy_print();

/* remove buddy block_num from the list of buddies with size = 2^exp */
void buddy_remove_block(int, int);

/* allocate size bytes */
void* bmalloc(int size);

/* free allocated memory */
int bfree(void*);