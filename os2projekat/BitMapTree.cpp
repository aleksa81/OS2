#include "BitMapTree.h"
#include <stdlib.h>
#include <assert.h>
#include "slab.h"

static unsigned buddy_N;

static char* bitmapTree;
static int bitmapTree_words_count;
static int bitmapTree_node_count;

void* bitmapTree_init(void* space, unsigned buddy_pow) {

	buddy_N = buddy_pow;
	bitmapTree = (char*)space;

	bitmapTree_words_count = ((1 << buddy_N + 1) * NODE_BITS) / WORD_BITS;
	bitmapTree_node_count = ((1 << buddy_N + 1) - 1);

	for (int i = 0; i < bitmapTree_words_count; i++) {
		bitmapTree[i] = 0;
	}

	return (void*)(bitmapTree + bitmapTree_words_count);
}

short bitmapTree_get_node(unsigned index) {
	/* O(1) */

	assert(index < bitmapTree_node_count);

	int word = index*NODE_BITS / WORD_BITS;
	int node = (index*NODE_BITS) % WORD_BITS;
	return (bitmapTree[word] >> (WORD_BITS - NODE_BITS - node)) & ((1<<NODE_BITS)-1);
}

void bitmapTree_set_node(unsigned index, short value) {
	/* O(1) */

	assert(index < bitmapTree_node_count);
	assert(value >=0 && value < (1<<NODE_BITS));

	int word = index*NODE_BITS / WORD_BITS;
	int node = (index*NODE_BITS) % WORD_BITS;

	char clear_value = ~((0 | ((1 << NODE_BITS) - 1)) << (WORD_BITS - NODE_BITS - node));
	char set_value = ((0 | value) << (WORD_BITS - NODE_BITS - node));

	bitmapTree[word] &= clear_value;
	bitmapTree[word] |= set_value;
}

int bitmapTree_get_block_size(unsigned index) {
	/* O( log(number of blocks) ) */

	assert(index < bitmapTree_node_count);
	int i = 1;
	while (index >((1 << i) - 2)) i++;
	return buddy_N - i + 1;
}

int bitmapTree_dealloc(unsigned blockn) {
	/* O( log(number of blocks) ) */

	assert(blockn < (1 << buddy_N));

	int leaf = blockn + (1 << buddy_N) - 1;
	while (bitmapTree_get_node(leaf) != TAKEN) {
		assert(bitmapTree_get_node(leaf) == FREE);
		leaf = PARENT(leaf);
	}
	bitmapTree_set_node(leaf, FREE);
	return leaf;
}

void bitmapTree_alloc(unsigned blockn, int pow) {
	/* O( log(number of blocks) ) */

	assert(blockn < (1 << buddy_N));
	assert(pow >= 0 && pow <= buddy_N);

	int leaf = blockn + (1 << buddy_N) - 1;
	int hop;
	for (hop = 0; hop < pow; hop++) {
		assert(bitmapTree_get_node(leaf) == FREE);
		leaf = PARENT(leaf);
	}
	bitmapTree_set_node(leaf, TAKEN);

	while (leaf > 0) {
		assert(bitmapTree_get_node(PARENT(leaf)) != TAKEN);
		if (bitmapTree_get_node(PARENT(leaf)) == PARTLY_FREE) break;
		bitmapTree_set_node(PARENT(leaf), PARTLY_FREE);
		leaf = PARENT(leaf);
	}
}

int bitmapTree_get_buddy(unsigned index) {
	/* O(1) */

	if ((index & 1) == 0) return index - 1;
	else return index + 1;
}

int bitmapTree_is_buddy_free(unsigned index) {
	/* O(1) */

	return bitmapTree_get_node(bitmapTree_get_buddy(index)) == FREE;
}

int bitmapTree_get_block(unsigned index) {
	/* O( log(number of blocks) ) */

	int i = index;
	while (LEFT(i) < bitmapTree_node_count) {
		i = LEFT(i);
	}
	return i - (1 << buddy_N) + 1;
}

void bitmapTree_print() {
	/* O(number of blocks) */

	int exp = 0;
	for (int i = 0; i < bitmapTree_node_count; i++) {
		if (i == ((1 << exp) - 1)) {
			exp++;
			printf("| ");
		}
		printf("%d ", bitmapTree_get_node(i));
	}
	printf("|\n");
}

