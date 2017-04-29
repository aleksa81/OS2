#pragma once
#include "BitMapTree.h"

#define __BUDDY_BLOCK_SIZE (4) // in bytes
#define __BUDDY_N (4) // 2^__BUDDY_N blocks
#define __BUDDY_NEXT(X) *(int*)block(X) // reference

/* memory space */
char space[];

/* keeps information about free memory */
int buddy_blocks[];

/* returns pointer to nth block */
void* block(int n);

/* allocate 2^i continous memory blocks of size = __BUDDY_BLOCK_SIZE */
void* buddy_alloc(int i);

/* deallocates memory pointed by ptr */
int buddy_dealloc(void* ptr);

/* calls bitmapTree_init() and initializes buddy_blocks */
void buddy_init();

/* prints buddy info */
void buddy_print();

/* remove buddy block_num from the list of buddies with size = 2^exp */
void buddy_remove_block(int, int);

/* allocate size bytes */
void* bmalloc(int size);

/* free allocated memory */
int bfree(void*);