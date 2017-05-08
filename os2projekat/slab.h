#pragma once

#include <stdlib.h>
#include "Buddy.h"
#include <mutex>
#define BLOCK_SIZE (4096)
#define BLOCK_NUMBER (1000)
#define CACHE_L1_LINE_SIZE (64)
#define SLAB_SIZE(n) ((1<<n)*BLOCK_SIZE)

#define LEFT_OVER(num, pow, size, off) \
		(SLAB_SIZE(pow) - (1-off)*sizeof(kmem_slab_t) - (num)*(size+(1-off)*sizeof(int)))

#define INSUFFICIENT_SLAB_SPACE(num, pow, size, off) \
		((SLAB_SIZE(pow) - (1-off)*sizeof(kmem_slab_t)) < (num)*(size+(1-off)*sizeof(int)))

#define FREE_OBJS(slabp) ((int*)(((kmem_slab_t*)slabp)+1))
#define CACHE_NAME_LEN (20)
#define OBJECT_TRESHOLD ((BLOCK_SIZE)>>3) // 1/8 of block size

typedef struct kmem_slab_s kmem_slab_t;
typedef struct kmem_cache_s kmem_cache_t;

/* ----------------------------------------------------------- */
/* -------------------------- CACHE -------------------------- */
/* ----------------------------------------------------------- */

void kmem_init(void *space, int block_num);

/* Allocate cache */
kmem_cache_t* kmem_cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *));

/* Shrink cache (thread safe) */
int kmem_cache_shrink(kmem_cache_t *cachep); 

/* Allocate one object from cache (thread safe) */
void* kmem_cache_alloc(kmem_cache_t *cachep);

/* Deallocate one object from cache (thread safe) */
void kmem_cache_free(kmem_cache_t *cachep, void *objp);

/* Alloacate one small memory buffer (thread safe) */
void* kmalloc(size_t size);

/* Deallocate one small memory buffer (thread safe) */
void kfree(const void *objp);

/* Deallocate cache */
void kmem_cache_destroy(kmem_cache_t *cachep);

/* Print cache info (thread safe) */
void kmem_cache_info(kmem_cache_t *cachep);

/* Print error message (thread safe) */
int kmem_cache_error(kmem_cache_t *cachep);

/* ---------------------------------------------------------- */
/* -------------------------- UTIL -------------------------- */
/* ---------------------------------------------------------- */

/* Removes cache from global cache list and returns removed cache */
kmem_cache_t* cache_remove_from_list(kmem_cache_t* cachep);

/* Removes slab from *headp slab list and returns removed slab */
kmem_slab_t* slab_remove_from_list(kmem_slab_t** headp, kmem_slab_t* slabp);

/* Adds slab to *headp slab list */
void slab_add_to_list(kmem_slab_t** headp, kmem_slab_t* slabp);

/* Creates and returns new slab for cache cachep */
kmem_slab_t* new_slab(kmem_cache_t* cachep);

/* Allocates one object of slab */
void* slab_alloc(kmem_slab_t* slabp);

/* Frees one object from slab */
void slab_free(kmem_slab_t* slabp, void* objp);

/* Checks if object is on slab*/
int is_obj_on_slab(kmem_slab_t* slabp, void* objp);

/* Adds empty slab to cache */
void add_empty_slab(kmem_cache_t* cachep); // for debugging

/* Enters critical section for cache cachep */
void enter_cs(kmem_cache_t* cachep);

/* Leaves critical section for cache cachep */
void leave_cs(kmem_cache_t* cachep);

/* Constructs cache on the given address */
void kmem_cache_constructor(kmem_cache_t* cachep, const char* name, size_t size,
	void(*ctor)(void*),
	void(*dtor)(void*));

/* Initialize all size-N caches */
void static_caches_init();

/* Prints slab info */
void kmem_slab_info(kmem_slab_t* slabp); // for debugging

/* Ctor for small memory buffers */
void cache_sizes_ctor(void* mem);

/* Calculate slab size and number of objects per slab */
void kmem_cache_estimate(unsigned* pow, unsigned* num, int size,  int off);

/* Calls ctor/dtor on all objects on this slab */
void process_objects_on_slab(kmem_slab_t* slabp, void(*function)(void *));

/* Update btsm so that all blocks of slabp map to set_to */
void btsm_update(kmem_slab_t* slabp, kmem_slab_t* set_to);

/* Check if cachep->name is already taken */
int kmem_cache_check_name_availability(const char* name);