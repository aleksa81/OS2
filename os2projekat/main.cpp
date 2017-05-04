#include <stdio.h>
#include "Buddy.h"
#include <thread>
#include <chrono>
#include "slab.h"

using namespace std;

void need_objs(kmem_cache_t* mc) {

	printf("in thread\n");

	void* obj1 = kmem_cache_alloc(mc);
	void* obj2 = kmem_cache_alloc(mc);
	void* obj3 = kmem_cache_alloc(mc);
	void* obj4 = kmem_cache_alloc(mc);
	void* obj5 = kmem_cache_alloc(mc);
	void* obj6 = kmem_cache_alloc(mc);
	void* obj7 = kmem_cache_alloc(mc);
	void* obj8 = kmem_cache_alloc(mc);

	std::this_thread::sleep_for(std::chrono::seconds(1));

	printf("woke up\n");

	kmem_cache_free(mc, obj1);
	kmem_cache_free(mc, obj2);
	kmem_cache_free(mc, obj3);
	kmem_cache_free(mc, obj4);
	kmem_cache_free(mc, obj5);
	kmem_cache_free(mc, obj6);
	kmem_cache_free(mc, obj7);
	kmem_cache_free(mc, obj8);

	printf("done\n");
}

int ctor_cnt = 0;
int dtor_cnt = 0;


void ctor(void* mem) {
	*(int*)mem = ctor_cnt++;
}

void dtor(void*mem) {
	//printf("dtor on object at address %d! #%d\n", (int)mem, dtor_cnt++);
}

int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);

	kmem_init(space, BLOCK_NUMBER);
	
	char buffer[1024];
	unsigned int size = 512;
	sprintf_s(buffer, 1024, "size-%d", size);

	kmem_cache_t* mc = kmem_cache_create(buffer, size, ctor, dtor);

	
	std::thread t1(need_objs, mc);
	std::thread t2(need_objs, mc);
	//std::thread t3(need_objs, mc);
	//std::thread t4(need_objs, mc);

	t1.join();
	t2.join();
	//t3.join();
	//t4.join();
	

	kmem_cache_shrink(mc);
	kmem_cache_shrink(mc);

	kmem_cache_info(mc);

	kmem_cache_destroy(mc);

	buddy_print();

	return 0;
}