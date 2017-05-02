#include <stdio.h>
#include "Buddy.h"
#include <thread>
#include <chrono>
#include "slab.h"

using namespace std;

void need_memory() {
	void * ptr1 = bmalloc(4);
	void * ptr2 = bmalloc(4);
	void * ptr3 = bmalloc(4);
	void * ptr4 = bmalloc(4);

	std::this_thread::sleep_for(std::chrono::seconds(3));

	bfree(ptr1);
	bfree(ptr2);

	void *ptr5 = bmalloc(4);
	void *ptr6 = bmalloc(4);

	std::this_thread::sleep_for(std::chrono::seconds(1));

	bfree(ptr3);
	bfree(ptr4);

	void *ptr7 = bmalloc(4);
	void *ptr8 = bmalloc(4);

	std::this_thread::sleep_for(std::chrono::seconds(1));

	bfree(ptr5);
	bfree(ptr6);
	bfree(ptr7);
	bfree(ptr8);
}

int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);

	kmem_init(space, BLOCK_NUMBER);
	buddy_print();
	
	char buffer[1024];
	unsigned int size = 512;
	sprintf_s(buffer, 1024, "size-%d", size);

	//kmem_cache_create(buffer, size, nullptr, nullptr);

	return 0;
}