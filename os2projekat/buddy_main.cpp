#include <stdio.h>
#include "Buddy.h"
#include <thread>
#include <chrono>
#include "slab.h"

#define BLOCK_NUMBER (8)

#ifdef BUDDY_MAIN

int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);

	buddy_init(space, BLOCK_NUMBER);

	void* ptr1 = bmalloc(64);
	if (ptr1 == nullptr) printf("not enough memory!\n");

	void* ptr2 = bmalloc(64);
	if (ptr2 == nullptr) printf("not enough memory!\n");

	void* ptr3 = bmalloc(64);
	if (ptr3 == nullptr) printf("not enough memory!\n");

	void* ptr4 = bmalloc(64);
	if (ptr4 == nullptr) printf("not enough memory!\n");

	void* ptr5 = bmalloc(64);
	if (ptr5 == nullptr) printf("not enough memory!\n");

	void* ptr6 = bmalloc(64);
	if (ptr6 == nullptr) printf("not enough memory!\n");

	void* ptr7 = bmalloc(64);
	if (ptr7 == nullptr) printf("not enough memory!\n");

	void* ptr8 = bmalloc(64);
	if (ptr8 == nullptr) printf("not enough memory!\n");

	buddy_print();

	bfree(ptr1);
	bfree(ptr3);
	bfree(ptr5);
	bfree(ptr7);
	bfree(ptr2);
	bfree(ptr4);
	bfree(ptr6);
	bfree(ptr8);

	buddy_print();
	
	return 0;
}

#endif

