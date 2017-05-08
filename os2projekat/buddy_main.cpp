#include <stdio.h>
#include "Buddy.h"
#include <thread>
#include <chrono>
#include "slab.h"

int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);

	kmem_init(space, BLOCK_NUMBER);

	buddy_print();

	return 0;
}

