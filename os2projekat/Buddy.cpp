#include "Buddy.h"
#include "BitMapTree.h"
#include <stdio.h>
#include <assert.h>
#include <mutex>

std::mutex buddy_mutex;

extern const int N;

extern char space[];
int buddy_blocks[__BUDDY_N + 1];

void* block(int n) {
	/* returns the pointer to a block number n */
	if (n >= 0 && n <= 1 << __BUDDY_N) {
		return space + n*__BUDDY_BLOCK_SIZE;
	}
	else return nullptr;
}

void buddy_init() {
	bitmapTree_init();
	for (int i = 0; i < __BUDDY_N; i++) buddy_blocks[i] = -1;
	buddy_blocks[__BUDDY_N] = 0;
	__BUDDY_NEXT(0) = -1;
}

void buddy_remove_block(int block_num, int exp) {
	/* O(number of blocks) */

	int *head = &buddy_blocks[exp];
	while (*head != block_num) {
		head = &__BUDDY_NEXT(*head);
		/* must not happen ! */
		assert(*head != -1);
	}
	*head = __BUDDY_NEXT(*head);
}

void* bmalloc(int size_in_bytes) {
	assert(size_in_bytes > 0);
	int buddy_size = __BUDDY_BLOCK_SIZE;
	int pow = 0;
	while (buddy_size < size_in_bytes) {
		buddy_size <<= 1;
		pow++;
	}
	return buddy_alloc(pow);
}

int bfree(void* block_ptr) {
	return buddy_dealloc(block_ptr);
}

void* buddy_alloc(int i) {
	/* O(log(number of blocks)) */

	std::lock_guard<std::mutex> lock(buddy_mutex);

	if (i <= __BUDDY_N) {
		int j = i;

		/* find bigger block if it exists */
		while (j <= __BUDDY_N && buddy_blocks[j] == -1) j++;

		/* if it doesn't exist return an error */
		if (j > __BUDDY_N) {
			printf("BUDDY: Out of memory error!");
			return nullptr;
		}

		/* split segment(s) in two halves */
		while (i != j) {

			buddy_blocks[j - 1] = buddy_blocks[j];

			/* it now points to next block of it' size */
			buddy_blocks[j] = __BUDDY_NEXT(buddy_blocks[j]);

			/* link two children blocks */
			__BUDDY_NEXT(buddy_blocks[j - 1]) = (int)(buddy_blocks[j - 1] + (1 << j - 1));
			__BUDDY_NEXT(buddy_blocks[j - 1] + (1 << j - 1)) = -1;

			/* update variables for next iteration */
			j--;
		}

		/* return pointer to allocated memory and update buddy arrays */
		void* mem = block(buddy_blocks[i]);
		bitmapTree_alloc_block(buddy_blocks[i],i);
		buddy_blocks[i] = __BUDDY_NEXT(buddy_blocks[i]);
		return mem;
	}

	/* if requested memory is larger then 2^__BUDDY_N */
	printf("BUDDY: Out of memory error!");
	buddy_mutex.unlock();
	return nullptr;
}

int buddy_dealloc(void * block_ptr) {
	/* O(number of blocks) */

	std::lock_guard<std::mutex> lock(buddy_mutex);

	if (block_ptr == nullptr) return 1;

	/* block_num is a number of the first block in the chunk of memory pointed by block_ptr */
	int block_num = (int)(((char*)block_ptr - space) / __BUDDY_BLOCK_SIZE);

	/* check block_ptr validity */
	assert(block_num >= 0 && block_num < (1 << __BUDDY_N));

	/* index = f(block_num, size of memory block that is beeing deallocated) */
	int index = bitmapTree_dealloc_block(block_num);

	/* size of black that is beeing deallocated */
	int block_size = bitmapTree_get_index_block_size(index);

	/* parent node of node(index) in bitmapTree structure */
	int index_parent = bitmapTree_get_parent(index);

	/* while it's possible to merge two buddies */
	while (index > 0 && bitmapTree_is_buddy_free(index)) {

		/* delete buddy from free blocks list */
		buddy_remove_block(bitmapTree_get_block_num(bitmapTree_get_buddy(index)), block_size);

		/* update variables for next iteration */
		index = index_parent;
		block_num = bitmapTree_get_block_num(index);
		block_size = bitmapTree_get_index_block_size(index);
		index_parent = bitmapTree_get_parent(index);
	}

	/* link new memory block to the list of free blocks */
	__BUDDY_NEXT(block_num) = buddy_blocks[block_size];
	buddy_blocks[block_size] = block_num;
	return 0;
}

void buddy_print() {
	/* prints buddy info */
	bitmapTree_print();

	for (int i = __BUDDY_N; i >= 0; i--) {
		printf("2^%d : %d", i, buddy_blocks[i]);
		int ptr = buddy_blocks[i];
		while (ptr != -1) {
			printf(" ");
			ptr = __BUDDY_NEXT(ptr);
		}
		printf("\n");
	}
	printf("\n");
}


