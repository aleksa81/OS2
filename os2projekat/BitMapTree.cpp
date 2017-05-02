#include "BitMapTree.h"
#include <stdlib.h>
#include <assert.h>

char bitmap[(1 << (BITS_TO_REPRESENT(BLOCK_NUMBER) - ISPOW2(BLOCK_NUMBER)) + 1) / BITMAP_BITS_PER_WORD];
int bitmap_words_count;
int bitmap_bits_count;

extern int buddy_N;

void bitmapTree_init() {
	/* O(number of blocks) */

	bitmap_words_count = sizeof(bitmap);
	bitmap_bits_count = ((1 << buddy_N + 1) - 1);

	printf("Init\n");
	for (int i = 0; i < bitmap_words_count; i++) {
		bitmap[i] = 0;
	}
}

short bitmapTree_getbit(unsigned index) {
	/* O(1) */

	assert(index < bitmap_bits_count);
	int word = index / BITMAP_BITS_PER_WORD;
	int bit = index % BITMAP_BITS_PER_WORD;
	return (bitmap[word] >> (BITMAP_BITS_PER_WORD - 1 - bit)) & 1;
}

void bitmapTree_setbit(unsigned index, short value) {
	/* O(1) */

	assert(index < bitmap_bits_count);
	assert(value == 0 || value == 1);
	char mask = 1 << (BITMAP_BITS_PER_WORD - index % BITMAP_BITS_PER_WORD - 1);
	if (value == 0) {
		mask = ~ mask;
		bitmap[index/BITMAP_BITS_PER_WORD] &= mask;
	}
	else bitmap[index / BITMAP_BITS_PER_WORD] |= mask;
}

int bitmapTree_get_block_size(unsigned index) {
	/* O( log(number of blocks) ) */

	assert(index < bitmap_bits_count);
	int i = 1;
	while (index >((1 << i) - 2)) i++;
	return buddy_N - i + 1;
}

int bitmapTree_dealloc(unsigned block_num) {
	/* O( log(number of blocks) ) */

	assert(block_num < (1 << buddy_N));
	int leaf = block_num + (1 << buddy_N) - 1;
	while (bitmapTree_getbit(leaf) != 1) {
		leaf = bitmapTree_get_parent(leaf);
	}
	assert(leaf >= 0);
	bitmapTree_setbit(leaf, 0);
	return leaf;
}

void bitmapTree_alloc(unsigned block_num, int pow) {
	/* O( log(number of blocks) ) */

	assert(block_num < (1 << buddy_N));
	int leaf = block_num + (1 << buddy_N) - 1;
	while (bitmapTree_get_block_size(leaf) < pow) {
		leaf = bitmapTree_get_parent(leaf);
	}
	assert(leaf >= 0);
	bitmapTree_setbit(leaf,1);
}

int bitmapTree_is_subtree_free(unsigned index) {
	/* O(number of blocks) */

	if (index >= bitmap_bits_count) return 1;
	if (bitmapTree_getbit(index) == 1) return 0;
	else return bitmapTree_is_subtree_free((index << 1) + 1) && bitmapTree_is_subtree_free((index << 1) + 2);
}

int bitmapTree_get_parent(unsigned index) {
	/* O(1) */

	return (index - 1) >> 1;
}

int bitmapTree_get_buddy(unsigned index) {
	/* O(1) */

	if ((index & 1) == 0) return index - 1;
	else return index + 1;
}

int bitmapTree_is_buddy_free(unsigned index) {
	/* O(number of blocks) */

	return bitmapTree_is_subtree_free(bitmapTree_get_buddy(index));
}

int bitmapTree_get_block(unsigned index) {
	/* O( log(number of blocks) ) */

	int i = index;
	while (((i << 1) + 1) < bitmap_bits_count) {
		i = (i << 1) + 1;
	}
	return i - (1 << buddy_N) + 1;
}

void bitmapTree_print() {
	/* O(number of blocks) */

	int exp = 0;
	for (int i = 0; i < bitmap_bits_count; i++) {
		if (i == ((1 << exp) - 1)) {
			exp++;
			printf("| ");
		}
		printf("%d ", bitmapTree_getbit(i));
	}
	printf("|\n");
}

