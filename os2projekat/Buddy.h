#pragma once
#include "BitMapTree.h"

#define __BUDDY_BLOCK_SIZE (4) // in bytes
#define __BUDDY_N (4) // 2^__BUDDY_N blocks
#define __BUDDY_NEXT(X) *(int*)block(X) // reference

char space[]; /* memory space */
int buddy_blocks[]; /* keeps information about free memory */

void* block(int);

/* allocate 2^i continous memory blocks of size = __BUDDY_BLOCK_SIZE */
void* buddy_alloc(int i);

int buddy_dealloc(void*);

void buddy_init();

void buddy_print();

/* remove buddy block_num from the list of buddies with size = 2^exp */
void buddy_remove_block(int, int);

/* allocate size bytes */
void* bmalloc(int size);

/* free allocated memory */
int bfree(void*);