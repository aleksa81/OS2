#include <stdio.h>
#include "Buddy.h"
#include <thread>
#include <chrono>
#include "slab.h"

#define _size 10

using namespace std;

//#define MY_MAIN

void need_objs(kmem_cache_t* mc) {

	void* objs[_size];

	for (int i = 0; i < _size; i++) {
		objs[i] = kmem_cache_alloc(mc);
		if (i%10==0) std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	//kmem_cache_info(mc);

	for (int i = 0; i < _size; i++) {
		kmem_cache_free(mc, objs[i]);
		if (i % 10 == 0) std::this_thread::sleep_for(std::chrono::seconds(1));
	}

}

int ctor_cnt = 0;
int dtor_cnt = 0;

void ctor(void* mem) {
	*(int*)mem = ctor_cnt++;
}

void dtor(void*mem) {
	//printf("dtor on object at address %d! #%d\n", (int)mem, dtor_cnt++);
}

#ifdef MY_MAIN

int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);

	kmem_init(space, BLOCK_NUMBER);
	
	char buffer[1024];
	unsigned int size = 600;

	sprintf_s(buffer, 1024, "my size-%d", size);

	kmem_cache_t* mc = kmem_cache_create(buffer, size, ctor, dtor);

	std::thread t1(need_objs, mc);
	std::thread t2(need_objs, mc);
	std::thread t3(need_objs, mc);
	std::thread t4(need_objs, mc);
	std::thread t5(need_objs, mc);
	std::thread t6(need_objs, mc);
	std::thread t7(need_objs, mc);
	std::thread t8(need_objs, mc);
	std::thread t9(need_objs, mc);
	std::thread t10(need_objs, mc);

	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();
	t6.join();
	t7.join();
	t8.join();
	t9.join();
	t10.join();

	kmem_cache_info(mc);
	
	kmem_cache_destroy(mc);

	buddy_print();
	
	return 0;
}

#endif
