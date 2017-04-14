#include "Buddy.h"
#include <cmath>
#include <iostream>
#include <assert.h>

char space[__BUDDY_BLOCK_SIZE*(1 << __BUDDY_N)]; 
int buddy_blocks[__BUDDY_N + 1];
int buddy_manager[1 << __BUDDY_N];

void * block(int n) {
	/* returns the pointer to a block number n */
	if (n >= 0 && n <= 1 << __BUDDY_N) {
		return space + n*__BUDDY_BLOCK_SIZE;
	}
	else return nullptr;
}

void buddy_init() {
	for (int i = 0; i < __BUDDY_N; i++) buddy_blocks[i] = -1;
	for (int i = 0; i < (1 << __BUDDY_N); i++) buddy_manager[i] = -1;
	buddy_blocks[__BUDDY_N] = 0;
	__BUDDY_NEXT(0) = -1;
}

int buddy_get_buddy(int block_num, int align) {
	/* returns first block's number in buddy of n */
	if (align == block_num) return block_num + __BUDDY_SIZE(block_num);
	else return align;
}

int buddy_is_free(int block, int size) {
	for (int i = block; i < block + size; i++) {
		if (buddy_manager[i] != -1) return 0;
	}
	return 1;
}

void buddy_print() {
	/* prints buddy info */
	for (int i = 0; i < (1 << __BUDDY_N); i++) {
		std::cout << buddy_manager[i] << " ";
	}
	std::cout << std::endl;

	for (int i = __BUDDY_N; i >= 0; i--) {
		std::cout << "2^" << i << " :";
		int ptr = buddy_blocks[i];
		while (ptr != -1) {
			std::cout << " " << ptr;
			ptr = __BUDDY_NEXT(ptr);
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

void buddy_remove_block(int block_num, int exp) {
	int *head = &buddy_blocks[exp];
	while (*head != block_num) {
		head = &__BUDDY_NEXT(*head);
		/* must not happen ! */
		assert(*head != -1);
	}
	*head = __BUDDY_NEXT(*head);
}

void * buddy_alloc(int i) {
	if (i <= __BUDDY_N) {
		int j = i;

		/* find bigger block if it exists */
		while (j <= __BUDDY_N && buddy_blocks[j] == -1) j++;

		/* if it doesn't exist return an error */
		if (j > __BUDDY_N) {
			std::cout << "BUDDY: Out of memory error!" << std::endl;
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
		buddy_manager[buddy_blocks[i]] = i;
		buddy_blocks[i] = __BUDDY_NEXT(buddy_blocks[i]);
		return mem;
	}

	/* if requested memory is larger then 2^__BUDDY_N */
	std::cout << "BUDDY: Out of memory error!" << std::endl;
	return nullptr;
}

int buddy_dealloc(void * block_ptr) {
	/* block_num is a number of the first block in the chunk of memory pointed by block_ptr */
	int block_num = (int)(((char*)block_ptr - space) / __BUDDY_BLOCK_SIZE);
	
	/* check block_ptr validity */
	assert(block_num >= 0 && block_num < (1 << __BUDDY_N));
	assert(__BUDDY_EXP(block_num) != -1);

	int align = __BUDDY_ALIGN(block_num);
	int my_buddy = buddy_get_buddy(block_num, align);

	/* merge buddies: until it's not merged up to limit && while buddy of block_num is free */
	while (__BUDDY_EXP(align)<__BUDDY_N && buddy_is_free(my_buddy, __BUDDY_SIZE(block_num))) { 

		/* remove buddy from coreesponding list */
		buddy_remove_block(my_buddy, __BUDDY_EXP(block_num));

		/* when merged size doubles */
		__BUDDY_EXP(align) = __BUDDY_EXP(block_num) + 1;

		/* update buddy manager if needed */
		if (block_num != align) buddy_manager[block_num] = -1;

		/* update variables for next iteration */
		block_num = align;
		align = __BUDDY_ALIGN(block_num);
		my_buddy = buddy_get_buddy(block_num, align);
	}

	/* add to the beginning of the corresponding list */
	__BUDDY_NEXT(block_num) = buddy_blocks[__BUDDY_EXP(block_num)];
	buddy_blocks[__BUDDY_EXP(block_num)] = block_num;
	buddy_manager[block_num] = -1;
	return 0;
}

void *balloc(int size) {
	assert(size > 0);
	int buddy_size = __BUDDY_BLOCK_SIZE;
	int pow = 0;
	while (buddy_size < size) {
		buddy_size <<= 1;
		pow++;
	}
	return buddy_alloc(pow);
}

int bfree(void* block_ptr) {
	return buddy_dealloc(block_ptr);
}
