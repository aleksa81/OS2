#include "Buddy.h"
#include "BitMapTree.h"
#include <stdio.h>
#include <assert.h>
#include <mutex>
#include <cmath>

int buddy_N;
void* buddy_space;
std::mutex buddy_mutex;

int buddy_blocks[BUDDY_N +1];

void* block(int n) {
	/* returns the pointer to a block number n */
	if (n >= 0 && n <= 1 << buddy_N) {
		return (void*)((int)buddy_space + n*BLOCK_SIZE);
	}
	else return nullptr;
}

void buddy_init(void * space, int n){

	buddy_space = space;
	buddy_N = BUDDY_N;
	bitmapTree_init();

	for (int i = 0; i < buddy_N; i++) buddy_blocks[i] = -1;
	buddy_blocks[buddy_N] = 0;
	NEXT(0) = -1;
}

void buddy_remove_block(int block_num, int exp) {
	/* O(number of blocks) */

	int *head = &buddy_blocks[exp];
	while (*head != block_num) {
		head = &NEXT(*head);
		/* must not happen ! */
		assert(*head != -1);
	}
	*head = NEXT(*head);
}

void* bmalloc(int size_in_bytes) {
	assert(size_in_bytes > 0);
	int buddy_size = BLOCK_SIZE;
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

	if (i <= buddy_N) {
		int j = i;

		/* find bigger block if it exists */
		while (j <= buddy_N && buddy_blocks[j] == -1) j++;

		/* if it doesn't exist return an error */
		if (j > buddy_N) {
			//printf("BUDDY: Out of memory error!");
			return nullptr;
		}

		/* split segment(s) in two halves */
		while (i != j) {

			buddy_blocks[j - 1] = buddy_blocks[j];

			/* it now points to next block of it' size */
			buddy_blocks[j] = NEXT(buddy_blocks[j]);

			/* link two children blocks */
			if (buddy_blocks[j - 1] + (1 << (j - 1)) >= BLOCK_NUMBER) {

				/* if the second block(buddy) is off limit, fake alloc it */
#ifdef BUDDY_DEBUG
				printf("BUDDY: skipping linkage!\n");
#endif
				bitmapTree_alloc(buddy_blocks[j - 1] + (1 << j - 1), j - 1);
				NEXT(buddy_blocks[j - 1]) = -1;
			}
			else {
				NEXT(buddy_blocks[j - 1]) = (int)(buddy_blocks[j - 1] + (1 << j - 1));
				NEXT(buddy_blocks[j - 1] + (1 << j - 1)) = -1;
			}
			
			/* update variables for next iteration */
			j--;
		}

		/* if there are blocks that are off limit within chosen block */
		if (buddy_blocks[j] + (1 << j) - 1 >= BLOCK_NUMBER) {
#ifdef BUDDY_DEBUG
			printf("BUDDY: bad block!\n");
#endif
			return nullptr;
		}

		/* return pointer to allocated memory and update buddy arrays */
		void* mem = block(buddy_blocks[i]);
		bitmapTree_alloc(buddy_blocks[i], i);
		buddy_blocks[i] = NEXT(buddy_blocks[i]);
		return mem;
	}

	/* if requested memory is larger then 2^__BUDDY_N */
	//printf("BUDDY: Out of memory error!");
	return nullptr;
}

int buddy_dealloc(void * block_ptr) {
	/* O(number of blocks) */

	std::lock_guard<std::mutex> lock(buddy_mutex);

	if (block_ptr == nullptr) return 1;

	/* block_num is a number of the first block in the chunk of memory pointed by block_ptr */
	int block_num = (int)(((char*)block_ptr - buddy_space) / BLOCK_SIZE);

	/* check block_ptr validity */
	assert(block_num >= 0 && block_num < (1 << buddy_N));

	/* index = f(block_num, size of memory block that is beeing deallocated) */
	int index = bitmapTree_dealloc(block_num);

	/* size of block that is beeing deallocated */
	int block_size = bitmapTree_get_block_size(index);

	/* while it's possible to merge two buddies */
	while (index > 0 && bitmapTree_is_buddy_free(index)) {

		/* delete buddy from free blocks list */
		buddy_remove_block(bitmapTree_get_block(bitmapTree_get_buddy(index)), block_size);

		/* update variables for next iteration */
		block_size++;
		index = bitmapTree_get_parent(index);
		block_num = bitmapTree_get_block(index);
	}

	/* link new memory block to the list of free blocks */
	NEXT(block_num) = buddy_blocks[block_size];
	buddy_blocks[block_size] = block_num;
	return 0;
}

void buddy_print() {
	/* prints buddy info */
	//bitmapTree_print();

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


