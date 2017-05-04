#include "slab.h"
#include <string.h>
#include <assert.h>
#include <windows.h>
#include <mutex>

kmem_cache_t* cache_head;

typedef struct kmem_slab_s {
	struct kmem_slab_s* next_slab;
	struct kmem_slab_s* prev_slab;
	struct kmem_cache_s* my_cache;
	unsigned int my_colour;
	unsigned int inuse;
	unsigned int free;
	void* objs;
}kmem_slab_t;

typedef struct kmem_cache_s {
	struct kmem_cache_s* next_cache; //
	struct kmem_cache_s* prev_cache; //

	kmem_slab_t* full;
	kmem_slab_t* partial;
	kmem_slab_t* empty;

	size_t obj_size; //
	mutable CRITICAL_SECTION cache_cs;
	mutable std::mutex my_mutex;
	unsigned int slab_size_power;
	unsigned int num_of_slabs; //
	unsigned int objs_per_slab;
	unsigned int colour_num;
	unsigned int colour_next;
	unsigned int growing; // set it to 1 when cache is expanded, set it to 0 when cache is shrinked
	char name[__CACHE_NAME_LEN]; //
	void(*ctor)(void*); //
	void(*dtor)(void*); //

} kmem_cache_t;

kmem_cache_t* cache_remove_from_list(kmem_cache_t* cachep) {
	if (cachep == nullptr) return nullptr;
	if (cachep->prev_cache != nullptr) cachep->prev_cache->next_cache = cachep->next_cache;
	if (cachep->next_cache != nullptr) cachep->next_cache->prev_cache = cachep->prev_cache;
	if (cachep == cache_head) cache_head = cache_head->next_cache;
	cachep->next_cache = nullptr;
	cachep->prev_cache = nullptr;
	return cachep;
}

kmem_slab_t* slab_remove_from_list(kmem_slab_t** headp, kmem_slab_t* slabp) {
	if (slabp == nullptr) return nullptr;
	if (slabp->prev_slab != nullptr) slabp->prev_slab->next_slab = slabp->next_slab;
	if (slabp->next_slab != nullptr) slabp->next_slab->prev_slab = slabp->prev_slab;
	if (slabp == (*headp)) *headp = (*headp)->next_slab;
	slabp->next_slab = nullptr;
	slabp->prev_slab = nullptr;
	return slabp;
}

void slab_add_to_list(kmem_slab_t** headp, kmem_slab_t* slabp) {
	if (slabp == nullptr) return;
	slabp->next_slab = *headp;
	slabp->prev_slab = nullptr;
	*headp = slabp;
}

kmem_slab_t* new_slab(kmem_cache_t* cachep) {
	/* returns new slab for cache cachep */

	kmem_slab_t* slabp = (kmem_slab_t*)bmalloc(BLOCK_SIZE*(1 << (cachep->slab_size_power)));

#ifdef SLAB_DEBUG
	printf("allocated %d blocks\n", (1 << (cachep->slab_size_power)));
#endif

	slabp->my_cache = cachep;
	slabp->my_colour = cachep->colour_next;
	cachep->colour_next = (++(cachep->colour_next) % cachep->colour_num);
	slabp->inuse = 0;
	slabp->free = 0;

	/* cache is growing */
	cachep->growing = 1;

#ifdef SLAB_DEBUG
	printf("slab start %d\n", (int)slabp - (int)slabp);
	printf("slab end %d\n", (int)slabp - (int)slabp + sizeof(kmem_slab_t));
#endif

	/* init array of indexes of free objects */
	for (int i = 0; i < cachep->objs_per_slab - 1; i++) {
		*(farray(slabp) + i) = i + 1;
	}
	*(farray(slabp) + cachep->objs_per_slab - 1) = -1;
	slabp->objs = (void*)((unsigned)(farray(slabp) + cachep->objs_per_slab)
		+ (slabp->my_colour)*CACHE_L1_LINE_SIZE);

#ifdef SLAB_DEBUG
	printf("slab array start %d\n", (unsigned)farray(slabp) - (unsigned)slabp);

	for (int i = 0; i < cachep->objs_per_slab; i++) {
		printf("%d ", *(farray(slabp) + i));
	}
	printf("\n");
	printf("slab array end %d\n", (unsigned)farray(slabp) - (unsigned)slabp + 4 * cachep->objs_per_slab);
	printf("slab objs start %d\n", (unsigned)slabp->objs - (unsigned)slabp);
#endif

	/* init all objects on the slab */
	for (int i = 0; i < cachep->objs_per_slab; i++) {
		if (cachep->ctor != nullptr) {
			(cachep->ctor)((void*)((int)slabp->objs + i*cachep->obj_size));
		}
	}

#ifdef SLAB_DEBUG
	for (int i = 0; i < cachep->objs_per_slab; i++) {
		printf("%d - %d\n", ((int)slabp->objs + i*cachep->obj_size) - (unsigned)slabp, *(int*)((int)slabp->objs + i*cachep->obj_size));
	}
	printf("slab objs end %d\n", ((int)slabp->objs + cachep->objs_per_slab*cachep->obj_size) - (unsigned)slabp);

	printf("slab end %d\n\n", BLOCK_SIZE*(1 << (cachep->slab_size_power)));
#endif

	return slabp;
}

void* slab_alloc(kmem_slab_t* slabp) {
	if (slabp == nullptr) return nullptr;
	if (slabp->free == -1) return nullptr;
	void* objp = (void*)((int)slabp->objs + slabp->free*slabp->my_cache->obj_size);
	slabp->free = *(farray(slabp) + slabp->free);
	slabp->inuse++;
	return objp;
}

void slab_free(kmem_slab_t* slabp, void* objp) {
	if (slabp == nullptr) return;
	if (objp == nullptr) return;

	/* object must be on this slab */
	assert(is_obj_on_slab(slabp, objp));

	/* back to init state */
	slabp->my_cache->ctor(objp);

	unsigned objn = ((unsigned)objp - (unsigned)slabp->objs) / slabp->my_cache->obj_size;
	*(farray(slabp) + objn) = slabp->free;
	slabp->inuse--;
	slabp->free = objn;
}

int is_obj_on_slab(kmem_slab_t* slabp, void* objp) {
	if (slabp == nullptr || objp == nullptr) return 0;
	return (int)slabp <= (int)objp &&
		(int)objp <= ((int)slabp + (1 << slabp->my_cache->slab_size_power)*BLOCK_SIZE);
}

void add_empty_slab(kmem_cache_t* cachep) {
	/* for debugging */

	if (cachep == nullptr) return;
	kmem_slab_t* slabp = new_slab(cachep);
	slab_add_to_list(&cachep->empty, slabp);
}


/* ----------------------------------------------------------- */
/* -------------------------- CACHE -------------------------- */
/* ----------------------------------------------------------- */

kmem_cache_t *kmem_cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *)) {

	kmem_cache_t* my_cache = (kmem_cache_t*)bmalloc(sizeof(kmem_cache_t));
	if (my_cache == nullptr) return nullptr; // kmem_cache_error

	strcpy(my_cache->name, name);
	my_cache->obj_size = size;
	my_cache->ctor = ctor;
	my_cache->dtor = dtor;
	my_cache->growing = 0;
	my_cache->num_of_slabs = 0;

	my_cache->full = nullptr;
	my_cache->partial = nullptr;
	my_cache->empty = nullptr;

	/* init critical section of this cache */
	InitializeCriticalSection(&(my_cache->cache_cs));

	/* puts new cache at the beginning of the cache list */
	my_cache->next_cache = cache_head;
	my_cache->prev_cache = nullptr;
	if (cache_head != nullptr) cache_head->prev_cache = my_cache;
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
	my_cache->colour_num = LEFT_OVER(num, pow, size) / CACHE_L1_LINE_SIZE + 1;
#ifdef SLAB_DEBUG
	printf("left-over:%d 1/8:%d slab:%d\n", LEFT_OVER(num, pow, size), (SLAB_SIZE(pow) >> 3), sizeof(kmem_slab_t));
	printf("slab size:2^%d blocks, num:%d, colours:%d\n\n", pow, num, my_cache->colour_num);
#endif
	return my_cache;
}

void kmem_init(void *space, int block_num) {
	cache_head = nullptr;
	buddy_init(space, block_num);
}

int kmem_cache_shrink(kmem_cache_t *cachep) {

	if (cachep == nullptr) return 0;

	/* ENTER CS */
	enter_cs(cachep);

	if (cachep->growing == 1) {
		cachep->growing = 0;

		/* LEAVE CS */
		leave_cs(cachep);
		return 0;
	}
	if (cachep->empty != nullptr) {
		int cnt = 0;
		while (cachep->empty != nullptr) {
			kmem_slab_t* slabp = slab_remove_from_list(&cachep->empty, cachep->empty);

			/* destroy all objects on this slab */
			for (int i = 0; i < cachep->objs_per_slab; i++) {
				if (cachep->dtor != nullptr) {
					(cachep->dtor)((void*)((int)slabp->objs + i*cachep->obj_size));
				}
			}
			cachep->num_of_slabs--;

			bfree(slabp);
			cnt++;
		}
#ifdef SLAB_DEBUG
		printf("freed %d blocks\n", (1 << cachep->slab_size_power) * cnt);
#endif
		int freed_blocks_num = (1 << cachep->slab_size_power) * cnt;

		/* LEAVE CS */
		leave_cs(cachep);
		return freed_blocks_num;
	}

	/* LEAVE CS */
	leave_cs(cachep);
	return 0;
}

void* kmem_cache_alloc(kmem_cache_t *cachep) {

	if (cachep == nullptr) return nullptr;

	/* ENTER CS */
	enter_cs(cachep);

	kmem_slab_t* slabp;
	if (cachep->partial == nullptr) {
		/* if partial is nullptr */

		slabp = new_slab(cachep);
		slab_add_to_list(&cachep->partial, slabp);
		cachep->num_of_slabs++;
		void* objp = slab_alloc(slabp);

		/* LEAVE CS */
		leave_cs(cachep);
		return objp;
	}
	else {
		/* if partial is not nullptr */

		kmem_slab_t* slabp = cachep->partial;
		void* objp = slab_alloc(slabp);
		if (slabp->free == -1) {
			/* if the slab is now full move it to full list */

			slab_remove_from_list(&cachep->partial, slabp);
			slab_add_to_list(&cachep->full, slabp);
		}

		/* LEAVE CS */
		leave_cs(cachep);
		return objp;
	}
}

void kmem_cache_free(kmem_cache_t *cachep, void *objp) {

	if (cachep == nullptr) return;
	if (objp == nullptr) return;

	/* ENTER CS */
	enter_cs(cachep);

	kmem_slab_t* slabp = cachep->partial;
	while (slabp != nullptr) {
		/* check partially full list */
		if (is_obj_on_slab(slabp, objp)) {
			slab_free(slabp, objp);
			if (slabp->inuse == 0) {
				/* if the slab is now empty move it to empty list */
				slab_remove_from_list(&cachep->partial, slabp);
				slab_add_to_list(&cachep->empty, slabp);

				/* LEAVE CS */
				leave_cs(cachep);
				return;
			}
		}
		slabp = slabp->next_slab;
	}

	slabp = cachep->full;
	while (slabp != nullptr) {
		/* check full list */
		if (is_obj_on_slab(slabp, objp)) {
			slab_free(slabp, objp);

			/* the slab is not full anymore */
			slab_remove_from_list(&cachep->full, slabp);
			slab_add_to_list(&cachep->partial, slabp);

			/* LEAVE CS */
			leave_cs(cachep);
			return;
		}
		slabp = slabp->next_slab;
	}

	/* LEAVE CS */
	leave_cs(cachep);
	return;
}

void kmem_cache_destroy(kmem_cache_t *cachep) {
	if (cachep == nullptr) return;
	if (cachep->full != nullptr || cachep->partial != nullptr) return;
	kmem_cache_shrink(cachep);
	DeleteCriticalSection(&(cachep->cache_cs));
	bfree(cachep);
}

void kmem_cache_info(kmem_cache_t* cachep) {
	printf("FULL - %d\n", cachep->full);
	printf("PARTIAL - %d\n", cachep->partial);
	printf("EMPTY - %d\n", cachep->empty);
}

void enter_cs(kmem_cache_t* cachep) {
	//EnterCriticalSection(&(cachep->cache_cs));
	std::lock_guard<std::mutex> lock(cachep->my_mutex);
	printf("ENTER\n");
}

void leave_cs(kmem_cache_t* cachep) {
	//LeaveCriticalSection(&(cachep->cache_cs));
	//cachep->my_mutex.unlock();
	printf("LEAVE\n");
}
