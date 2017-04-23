#include <stdio.h>
#include "Buddy.h"

using namespace std;

char space[(__BUDDY_BLOCK_SIZE)*(1 << __BUDDY_N)];

int main() {

	buddy_init();

	void * ptr1 = bmalloc(7);
	void * ptr2 = bmalloc(6);
	void * ptr3 = bmalloc(4);
	void * ptr4 = bmalloc(4);
	void * ptr5 = bmalloc(4);
	void * ptr6 = bmalloc(4);
	
	buddy_print();

	bfree(ptr1);
	bfree(ptr2);
	bfree(ptr3);
	bfree(ptr4);
	bfree(ptr5);
	bfree(ptr6);

	buddy_print();

	return 0;
}