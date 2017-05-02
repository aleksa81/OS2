#include "slab.h"
#include "Buddy.h"
#include <string.h>

#define __CACHE_NAME_LEN (30)

kmem_cache_t* cache_head;

typedef struct kmem_slab_s {
	struct kmem_slab_s* next_slab; 
	struct kmem_cache_s* my_cache; 
	unsigned int my_color; 
	unsigned int inuse; 
	unsigned int free; 
	void* objs; 
}kmem_slab_t;

typedef struct kmem_cache_s {
	struct kmem_cache_s* next_cache; //

	kmem_slab_t* full;
	kmem_slab_t* partial;
	kmem_slab_t* empty;

	size_t obj_size; //
	unsigned int slab_size_power;
	unsigned int num_of_slabs; //
	unsigned int objs_per_slab;
	unsigned int colour_num;
	unsigned int colour_next;
	unsigned int growing; //
	char name[__CACHE_NAME_LEN]; //
	void(*ctor)(void*); //
	void(*dtor)(void*); //
} kmem_cache_t;


kmem_cache_t *kmem_cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *)) {

	kmem_cache_t* my_cache = (kmem_cache_t*)bmalloc(sizeof(kmem_cache_t));
	if (my_cache == nullptr) return nullptr;

	strcpy(my_cache->name, name);
	my_cache->obj_size = size;
	my_cache->ctor = ctor;
	my_cache->dtor = dtor;
	my_cache->growing = 0;
	my_cache->num_of_slabs = 0;

	/* puts new cache at the beginning of the cache list */
	my_cache->next_cache = cache_head;
	cache_head = my_cache;

	/* slab_size, objs_pre_slab, colour_num, colour_next */
	
	unsigned int pow = 0;
	unsigned int num = 1;

	while (!INSUFFICIENT_SLAB_SPACE(num + 1, pow, size)) num++;
	while (INSUFFICIENT_SLAB_SPACE(num, pow, size) || LEFT_OVER(num, pow, size)>(SLAB_SIZE(pow)>>3)){
		pow++;
		while (!INSUFFICIENT_SLAB_SPACE(num + 1, pow, size)) num++;
	}

	my_cache->slab_size_power = pow;
	my_cache->objs_per_slab = num;

	my_cache->colour_next = 0;
	my_cache->colour_num = LEFT_OVER(num, pow, size) / CACHE_L1_LINE_SIZE;

	printf("left-over:%d 1/8:%d\n", LEFT_OVER(num, pow, size), (SLAB_SIZE(pow) >> 3));
	printf("slab size:%d, num:%d, colours:%d\n", pow, num, my_cache->colour_num);

	return my_cache;
}

void kmem_cache_info(kmem_cache_t *cachep) {
	printf("%d\n", sizeof(kmem_slab_t));
}

void kmem_init(void *space, int block_num) {
	cache_head = nullptr;
	buddy_init(space, block_num);
}