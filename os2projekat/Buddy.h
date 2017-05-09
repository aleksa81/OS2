#pragma once

/* returns pointer to nth block */
void* block(int n);

/* allocate 2^i continous memory blocks of size = __BUDDY_BLOCK_SIZE */
void* buddy_alloc(int i);

/* deallocates memory pointed by ptr */
int buddy_dealloc(void* ptr);

/* calls bitmapTree_init() and initializes buddy_blocks */
void* buddy_init(void* space, int* block_number);

/* prints buddy info */
void buddy_print();

/* remove buddy blockn from the list of buddies with size = 2^pow */
int buddy_remove_block(int blockn, int pow);

/* add buddy blockn from the list of buddies with size = 2^pow */
void buddy_add_block(int blockn, int pow);

/* allocate size bytes */
void* bmalloc(int size);

/* free allocated memory */
int bfree(void*);