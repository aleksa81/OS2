#pragma once

#include <stdlib.h>
#include "Buddy.h"
#define BLOCK_SIZE (4096)
#define CACHE_L1_LINE_SIZE (64)
#define SLAB_SIZE(n) ((1<<n)*BLOCK_SIZE)
#define LEFT_OVER(num, pow, size) (SLAB_SIZE(pow) - sizeof(kmem_slab_t) - (num)*(size+4))
#define INSUFFICIENT_SLAB_SPACE(num, pow, size) ((SLAB_SIZE(pow) - sizeof(kmem_slab_t)) < (num)*(size+4))
#define farray(slabp) (int*)(((kmem_slab_t*)slabp)+1)
#define __CACHE_NAME_LEN (50)

//#define SLAB_DEBUG

typedef struct kmem_slab_s kmem_slab_t;
typedef struct kmem_cache_s kmem_cache_t;

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

/* Deallocate cache (thread safe) */
void kmem_cache_destroy(kmem_cache_t *cachep);

/* Print cache info */
void kmem_cache_info(kmem_cache_t *cachep);

/* Print error message */
int kmem_cache_error(kmem_cache_t *cachep);


kmem_cache_t* cache_remove_from_list(kmem_cache_t* cachep);

kmem_slab_t* slab_remove_from_list(kmem_slab_t** headp, kmem_slab_t* slabp);

void slab_add_to_list(kmem_slab_t** headp, kmem_slab_t* slabp);

kmem_slab_t* new_slab(kmem_cache_t* cachep);

void* slab_alloc(kmem_slab_t* slabp);

void slab_free(kmem_slab_t* slabp, void* objp);

int is_obj_on_slab(kmem_slab_t* slabp, void* objp);

void add_empty_slab(kmem_cache_t* cachep);

void enter_cs(kmem_cache_t* cachep);
void leave_cs(kmem_cache_t* cachep);


