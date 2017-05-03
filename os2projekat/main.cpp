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

int ctor_cnt = 0;
int dtor_cnt = 0;


void ctor(void* mem) {
	*(int*)mem = ctor_cnt++;
}

void dtor(void*mem) {
	printf("dtor on object at address %d! #%d\n", (int)mem, dtor_cnt++);
}

int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);

	kmem_init(space, BLOCK_NUMBER);
	
	char buffer[1024];
	unsigned int size = 512;
	sprintf_s(buffer, 1024, "size-%d", size);

	kmem_cache_t* mc = kmem_cache_create(buffer, size, ctor, dtor);

	/*add_empty_slab(mc);
	add_empty_slab(mc);
	add_empty_slab(mc);
	add_empty_slab(mc);

	kmem_cache_shrink(mc);
	kmem_cache_shrink(mc);*/

	void* obj1 = kmem_cache_alloc(mc);
	void* obj2 = kmem_cache_alloc(mc);
	void* obj3 = kmem_cache_alloc(mc);
	void* obj4 = kmem_cache_alloc(mc);
	void* obj5 = kmem_cache_alloc(mc);
	void* obj6 = kmem_cache_alloc(mc);
	void* obj7 = kmem_cache_alloc(mc);
	void* obj8 = kmem_cache_alloc(mc);


	kmem_cache_free(mc, obj1);
	kmem_cache_free(mc, obj2);
	kmem_cache_free(mc, obj3);
	kmem_cache_free(mc, obj4);
	kmem_cache_free(mc, obj5);
	kmem_cache_free(mc, obj6);
	kmem_cache_free(mc, obj7);
	kmem_cache_free(mc, obj8);

	kmem_cache_shrink(mc);
	kmem_cache_shrink(mc);

	kmem_cache_info(mc);

	buddy_print();

	return 0;
}