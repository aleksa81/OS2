#include "Buddy.h"
#include "BitMapTree.h"
#include <stdio.h>
#include <assert.h>
#include "slab.h"
#include <mutex>
#include <cmath>

#define NEXT(X) *(int*)block(X)      // reference
#define PREV(X) *((int*)block(X)+1)  // reference

static void* buddy_space;
static unsigned buddy_blocks_num;
static std::mutex buddy_mutex;

static unsigned buddy_N;

static int* buddy_blocks;

void* block(int n) {
	/* returns the pointer to a block number n */
	if (n >= 0 && n <= 1 << buddy_N) {
		return (void*)((int)buddy_space + n*BLOCK_SIZE);
	}
	else return nullptr;
}

void* buddy_init(void * space, int *block_number){

	/* assert MUST be true because on start of each block         */
	/* there are two pointers (ints) used for linking free blocks */

	assert(BLOCK_SIZE > 2 * sizeof(int));

	buddy_N = 0;
	while ((1 << buddy_N) < *block_number) buddy_N++;

	buddy_blocks = (int*)space;

	void* filled = bitmapTree_init((void*)((buddy_blocks + buddy_N + 1)), buddy_N);

	/* align with BLOCK_SIZE multiple */
	filled = (void*)(((unsigned)filled + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1));

	/* calculate how many blocks are lost for buddy and bitmap structs */
	unsigned lost_blocks = ((unsigned)filled - (unsigned)space) / BLOCK_SIZE;

	/* buddy blocks start from this address */
	buddy_space = filled;

	(*block_number) = (*block_number) - lost_blocks;

	buddy_blocks_num = *block_number;

	for (int i = 0; i <= buddy_N; i++) buddy_blocks[i] = -1;
	buddy_add_block(0, buddy_N);

	/* return starting address of blocks to kmem_init() */
	/* it's needed for block to slab mapping            */
	return filled;
}

int buddy_remove_block(int blockn, int pow) {
	/* O(1) */

	assert(blockn >= 0 && blockn < buddy_blocks_num);
	assert(pow >= 0 && pow<=buddy_N);

	if (NEXT(blockn) != -1) PREV(NEXT(blockn)) = PREV(blockn);
	if (PREV(blockn) != -1) NEXT(PREV(blockn)) = NEXT(blockn);

	/* if it was the first block in list */
	if (PREV(blockn) == -1) buddy_blocks[pow] = NEXT(blockn);
	return blockn;
}

void buddy_add_block(int blockn, int pow) {
	/* O(1) */

	assert(blockn >= 0 && blockn < buddy_blocks_num);
	assert(pow >= 0 && pow<=buddy_N);

	PREV(blockn) = -1;
	NEXT(blockn) = buddy_blocks[pow];
	if (buddy_blocks[pow] != -1) PREV(buddy_blocks[pow]) = blockn;
	buddy_blocks[pow] = blockn;
}

void* bmalloc(int size_in_bytes) {
	assert(size_in_bytes > 0);
	int pow = 0;
	while ((1<<pow)*BLOCK_SIZE < size_in_bytes) pow++;
	return buddy_alloc(pow);
}

int bfree(void* blockp) {
	if (blockp != nullptr) return buddy_dealloc(blockp);
	else return 1;
}

void* buddy_alloc(int i) {
	/* O(log(number of blocks)) */

	std::lock_guard<std::mutex> lock(buddy_mutex);

	int blockn;

	if (i <= buddy_N) {
		int j = i;

		/* find block to split if needed */
		while (j <= buddy_N && buddy_blocks[j] == -1) j++;

		/* if not enough memory return nullptr */
		if (j > buddy_N) return nullptr;
		
		/* split segment(s) in two halves */
		while (i != j) {

			blockn = buddy_remove_block(buddy_blocks[j], j);
	
			if (blockn + (1 << (j - 1)) >= buddy_blocks_num) {
				/* if the beginning of the second half of divided block is off limit, fake alloc it */
				bitmapTree_alloc(blockn + (1 << j - 1), j - 1);
			}
			else {
				/* else, add it to appropriate list of free blocks */
				buddy_add_block(blockn + (1 << j - 1), j - 1);
			}

			/* add first half of divided block to appropriate list of free blocks */
			buddy_add_block(blockn, j - 1);
			
			j--;
		}

		/* if there are blocks that are off limit within chosen block */
		if (buddy_blocks[j] + (1 << j) - 1 >= buddy_blocks_num) return nullptr;
		
		/* return pointer to allocated memory and update buddy arrays */
		blockn = buddy_remove_block(buddy_blocks[i], i);
		void* mem = block(blockn);
		bitmapTree_alloc(blockn, i);
		return mem;
	}

	return nullptr;
}

int buddy_dealloc(void * blockp) {
	/* O(number of blocks) */

	std::lock_guard<std::mutex> lock(buddy_mutex);

	/* block_num is a number of the first block in the chunk of memory pointed by block_ptr */
	int block_num = (int)(((char*)blockp - buddy_space) / BLOCK_SIZE);

	/* check block_ptr validity */
	assert(block_num >= 0 && block_num < (1 << buddy_N));

	int node = bitmapTree_dealloc(block_num);

	int block_size = bitmapTree_get_block_size(node);

	while (node > 0 && bitmapTree_is_buddy_free(node)) {

		buddy_remove_block
		(
			bitmapTree_get_block(bitmapTree_get_buddy(node)), 
			block_size
		);

		block_size++;
		node = PARENT(node);

		assert(bitmapTree_get_node(node) == PARTLY_FREE);
		bitmapTree_set_node(node, FREE);

		block_num = bitmapTree_get_block(node);
	}

	/* link new memory block to the list of free blocks */
	buddy_add_block(block_num, block_size);

	return 0;
}

void buddy_print() {
	/* prints buddy info */
	bitmapTree_print();

	for (int i = buddy_N; i >= 0; i--) {
		printf("2^%d : %d", i, buddy_blocks[i]);
		int ptr = buddy_blocks[i];
		while (ptr != -1) {
			ptr = NEXT(ptr);
			printf(" %d", ptr);
		}
		printf("\n");
	}
	printf("\n");
}


