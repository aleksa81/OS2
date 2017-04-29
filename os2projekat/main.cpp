#include <stdio.h>
#include "Buddy.h"
#include <thread>
#include <chrono>

using namespace std;

char space[(__BUDDY_BLOCK_SIZE)*(1 << __BUDDY_N)];

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

	buddy_init();

	std::thread t1(need_memory);
	std::thread t2(need_memory);
	//std::thread t3(need_memory);
	//std::thread t4(need_memory);

	t1.join();
	t2.join();
	//t3.join();
	//t4.join();
	
	buddy_print();

	return 0;
}