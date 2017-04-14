#pragma once
#define __BUDDY_BLOCK_SIZE (32)
#define __BUDDY_N (3) // 2^__BUDDY_N blocks
#define __BUDDY_NEXT(X) *(int*)block(X) // reference
#define __BUDDY_ALIGN(X) (X & ~0 << buddy_manager[X] + 1)
#define __BUDDY_EXP(X) buddy_manager[X]
#define __BUDDY_SIZE(X) (1<<buddy_manager[X])

char space[]; /* memory space */
int buddy_blocks[]; /* keeps information about free memory */
int buddy_manager[]; /* keeps information about allocated memory */

void* block(int);

/* allocate 2^i continous memory blocks of size = __BUDDY_BLOCK_SIZE */
void* buddy_alloc(int i);

int buddy_dealloc(void*);
void buddy_init();

/* returns block number of the buddy of block n*/
int buddy_get_buddy(int n);

/* check if buddy of n is free */
int buddy_is_free(int align, int n);

void buddy_print();

/* remove buddy block_num from the list of buddies with size = 2^exp */
void buddy_remove_block(int, int);

/* allocate size bytes */
void* balloc(int size);

/* free allocated memory */
int bfree(void*);
