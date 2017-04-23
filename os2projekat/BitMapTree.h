#pragma once
#include <stdio.h>
#include "Buddy.h"

#define __BITMAP_BITS_PER_WORD (sizeof(char)*8)
#define __BITMAP_WORDS_COUNT ((1 << (__BUDDY_N + 1))  / __BITMAP_BITS_PER_WORD)
#define __BITMAP_BITS_COUNT ((1 << (__BUDDY_N + 1))-1)

void bitmapTree_print();

short bitmapTree_getbit(unsigned index);

void bitmapTree_setbit(unsigned index,short value);

void bitmapTree_init();

int bitmapTree_get_index_block_size(unsigned index);

int bitmapTree_dealloc_block(unsigned block_num);

void bitmapTree_alloc_block(unsigned block_num, int pow);

int bitmapTree_is_subtree_free(unsigned index);

int bitmapTree_get_parent(unsigned index);

int bitmapTree_get_buddy(unsigned index);

int bitmapTree_is_buddy_free(unsigned index);

int bitmapTree_get_block_num(unsigned index);
