#pragma once
#include <stdio.h>
#include "Buddy.h"

#define __BITMAP_BITS_PER_WORD (sizeof(char)*8)

/* prints bitmapTree info */
void bitmapTree_print();

/* returns the value of bit at index */
short bitmapTree_getbit(unsigned index);

/* sets the value of bit at index */
void bitmapTree_setbit(unsigned index,short value);

/* sets all bits to 0 */
void bitmapTree_init(int buddy_n);

/* gets index level in bitmapTree */
int bitmapTree_get_block_size(unsigned index);

/* deallocates block in bitmapTree */
int bitmapTree_dealloc(unsigned block_num);

/* allocates memory of size 2^pow blocks */
void bitmapTree_alloc(unsigned block_num, int pow);

/* checks if memory represented by subtree with root index is free */
int bitmapTree_is_subtree_free(unsigned index);

/* return parent of index */
int bitmapTree_get_parent(unsigned index);

/* returns buddy of index */
int bitmapTree_get_buddy(unsigned index);

/* checks if buddy subtree is free */
int bitmapTree_is_buddy_free(unsigned index);

/* returns number of first block pointed by index */
int bitmapTree_get_block(unsigned index);

