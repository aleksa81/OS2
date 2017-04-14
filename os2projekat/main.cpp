#include <iostream>
#include "Buddy.h"


using namespace std;

int main() {
	
	buddy_init();

	void * ptr1 = buddy_alloc(0);
	void * ptr2 = buddy_alloc(0);
	void * ptr3 = buddy_alloc(0);
	void * ptr4 = buddy_alloc(0);

	void * ptr5 = buddy_alloc(0);
	void * ptr6 = buddy_alloc(0);
	void * ptr7 = buddy_alloc(0);
	void * ptr8 = buddy_alloc(0);

	
	
	bfree(ptr7);
	bfree(ptr5);
	bfree(ptr3);
	bfree(ptr1);

	bfree(ptr2);
	bfree(ptr4);
	bfree(ptr6);
	bfree(ptr8);
	
	buddy_print();

	return 0;
}