#pragma once
#include <stdio.h>
#include "Buddy.h"

#define FREE (0)
#define TAKEN (3)
#define PARTLY_FREE (1)

#define NODE_BITS (2)
#define WORD_BITS (sizeof(char)*8)

#define PARENT(node) ((node-1)>>1)
#define LEFT(node) ((node<<1)+1)
#define RIGHT(node) ((node<<1)+2)

/* prints bitmapTree info */
void bitmapTree_print();

/* returns the value of bit at index */
short bitmapTree_get_node(unsigned index);

/* sets the value of bit at index */
void bitmapTree_set_node(unsigned index,short value);

/* sets all bits to 0 */
void* bitmapTree_init(void* space, unsigned buddy_pow);

/* gets index level in bitmapTree */
int bitmapTree_get_block_size(unsigned index);

/* deallocates block in bitmapTree */
int bitmapTree_dealloc(unsigned block_num);

/* allocates memory of size 2^pow blocks */
void bitmapTree_alloc(unsigned block_num, int pow);

/* returns buddy of index */
int bitmapTree_get_buddy(unsigned index);

/* checks if buddy subtree is free */
int bitmapTree_is_buddy_free(unsigned index);

/* returns number of first block pointed by index */
int bitmapTree_get_block(unsigned index);

