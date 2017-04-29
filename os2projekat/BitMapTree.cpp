#include "BitMapTree.h"
#include <assert.h>

char bitmap[__BITMAP_WORDS_COUNT];

void bitmapTree_init() {
	/* O(number of blocks) */

	printf("Init\n");
	for (int i = 0; i < __BITMAP_WORDS_COUNT; i++) {
		bitmap[i] = 0;
	}
}

short bitmapTree_getbit(unsigned index) {
	/* O(1) */

	assert(index < __BITMAP_BITS_COUNT);
	int word = index / __BITMAP_BITS_PER_WORD;
	int bit = index % __BITMAP_BITS_PER_WORD;
	return (bitmap[word] >> (__BITMAP_BITS_PER_WORD - 1 - bit)) & 1;
}

void bitmapTree_setbit(unsigned index, short value) {
	/* O(1) */

	assert(index < __BITMAP_BITS_COUNT);
	assert(value == 0 || value == 1);
	char mask = 1 << (__BITMAP_BITS_PER_WORD - index % __BITMAP_BITS_PER_WORD - 1);
	if (value == 0) {
		mask = ~ mask;
		bitmap[index/__BITMAP_BITS_PER_WORD] &= mask;
	}
	else bitmap[index / __BITMAP_BITS_PER_WORD] |= mask;
}

int bitmapTree_get_block_size(unsigned index) {
	/* O( log(number of blocks) ) */

	assert(index < __BITMAP_BITS_COUNT);
	int i = 1;
	while (index >((1 << i) - 2)) i++;
	return __BUDDY_N - i + 1;
}

int bitmapTree_dealloc(unsigned block_num) {
	/* O( log(number of blocks) ) */

	assert(block_num < (1 << __BUDDY_N));
	int leaf = block_num + (1 << __BUDDY_N) - 1;
	while (bitmapTree_getbit(leaf) != 1) {
		leaf = bitmapTree_get_parent(leaf);
	}
	assert(leaf >= 0);
	bitmapTree_setbit(leaf, 0);
	return leaf;
}

void bitmapTree_alloc(unsigned block_num, int pow) {
	/* O( log(number of blocks) ) */

	assert(block_num < (1 << __BUDDY_N));
	int leaf = block_num + (1 << __BUDDY_N) - 1;
	while (bitmapTree_get_block_size(leaf) < pow) {
		leaf = bitmapTree_get_parent(leaf);
	}
	assert(leaf >= 0);
	bitmapTree_setbit(leaf,1);
}

int bitmapTree_is_subtree_free(unsigned index) {
	/* O(number of blocks) */

	if (index >= __BITMAP_BITS_COUNT) return 1;
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
	while (((i << 1) + 1) < __BITMAP_BITS_COUNT) {
		i = (i << 1) + 1;
	}
	return i - (1 << __BUDDY_N) + 1;
}

void bitmapTree_print() {
	/* O(number of blocks) */

	int exp = 0;
	for (int i = 0; i < __BITMAP_BITS_COUNT; i++) {
		if (i == ((1 << exp) - 1)) {
			exp++;
			printf("| ");
		}
		printf("%d ", bitmapTree_getbit(i));
	}
	printf("|\n");
}

