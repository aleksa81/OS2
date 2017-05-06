#include "slab.h"
#include <string.h>
#include <assert.h>
#include <windows.h> // CRITICAL_SECTION
#include <mutex>

kmem_cache_t* cache_head;

unsigned start;

/* all pointers are set to nullptr at the beginning */
kmem_slab_t* block_to_slab_mapping[BLOCK_NUMBER] = {nullptr};

typedef struct mem_buffer {
	size_t cs_size;
	kmem_cache_t* cs_cachep;
} mem_buffer_t;

static mem_buffer_t mem_buffer_array[] = {
		 {    32, nullptr }, // 5
		 {    64, nullptr }, // 6
		 {   128, nullptr }, // 7
		 {   256, nullptr }, // 8
		 {   512, nullptr }, // 9
		 {  1024, nullptr }, // 10
		 {  2048, nullptr }, // 11
		 {  4096, nullptr }, // 12
		 {  8192, nullptr }, // 13
		 { 16384, nullptr }, // 14
		 { 32768, nullptr }, // 15
		 { 65536, nullptr }, // 16
		 {131072, nullptr }  // 17
};

typedef struct kmem_slab_s {
	struct kmem_slab_s* next_slab; // initially nullptr
	struct kmem_slab_s* prev_slab; // initially nullptr
	struct kmem_cache_s* my_cache;
	unsigned int my_colour; // offset
	unsigned int inuse; // number of used objects 
	unsigned int free; // pointer to first free object 
	void* objs; // pointer to first object 
}kmem_slab_t;

typedef struct kmem_cache_s {
	struct kmem_cache_s* next_cache; // initially nullptr
	struct kmem_cache_s* prev_cache; // initially nullptr

	kmem_slab_t* full; // initially nullptr
	kmem_slab_t* partial; // initially nullptr
	kmem_slab_t* empty; // initially nullptr

	size_t obj_size; 

	/* per cache critical section (mutex) */
	mutable CRITICAL_SECTION cache_cs; 

	unsigned int slab_size;
	unsigned int num_of_slabs; 
	unsigned int objs_per_slab;
	unsigned int error;
	unsigned int colour_num;
	unsigned int colour_next;
	unsigned int num_of_active_objs;
	unsigned int off_slab;

	/* set it to 1 when cache is expanded, set it to 0 when cache is shrinked */
	unsigned int growing;

	void(*ctor)(void*); 
	void(*dtor)(void*); 
	char name[CACHE_NAME_LEN];

} kmem_cache_t;

/* ---------------------------------------------------------- */
/* -------------------------- UTIL -------------------------- */
/* ---------------------------------------------------------- */

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
	if (slabp == nullptr || *headp == nullptr) return nullptr;
	if (slabp->prev_slab != nullptr) (slabp->prev_slab)->next_slab = slabp->next_slab;
	if (slabp->next_slab != nullptr) (slabp->next_slab)->prev_slab = slabp->prev_slab;
	if (slabp == (*headp)) (*headp) = (*headp)->next_slab;
	slabp->next_slab = nullptr;
	slabp->prev_slab = nullptr;
	return slabp;
}

void slab_add_to_list(kmem_slab_t** headp, kmem_slab_t* slabp) {
	if (slabp == nullptr) return;
	slabp->next_slab = *headp;
	slabp->prev_slab = nullptr;
	if (*headp != nullptr) (*headp)->prev_slab = slabp;
	*headp = slabp;
}

void kmem_slab_info(kmem_slab_t* slabp) {
	if (slabp == nullptr) return;
	printf("\nallocated %d blocks\n", slabp->my_cache->slab_size);
	printf("slab start %d\n", (int)slabp - (int)slabp);
	printf("slab end %d\n", (int)slabp - (int)slabp + sizeof(kmem_slab_t));
	printf("slab array start %d\n", (unsigned)FREE_OBJS(slabp) - (unsigned)slabp);

	for (int i = 0; i < slabp->my_cache->objs_per_slab; i++) {
		printf("%d ", *(FREE_OBJS(slabp) + i));
	}
	printf("\n");
	printf("slab array end %d\n", (unsigned)FREE_OBJS(slabp) - (unsigned)slabp + 4 * slabp->my_cache->objs_per_slab);
	printf("slab objs start %d\n", (unsigned)slabp->objs - (unsigned)slabp);

	for (int i = 0; i < slabp->my_cache->objs_per_slab; i++) {
		printf("%d - %d\n", ((unsigned)slabp->objs + i*slabp->my_cache->obj_size) - (unsigned)slabp, 
							*(unsigned*)((unsigned)slabp->objs + i*slabp->my_cache->obj_size));
	}
	printf("slab objs end %d\n", ((unsigned)slabp->objs + slabp->my_cache->objs_per_slab*slabp->my_cache->obj_size) 
								- (unsigned)slabp);

	printf("slab end %d\n", BLOCK_SIZE*(slabp->my_cache->slab_size));
}

kmem_slab_t* new_slab(kmem_cache_t* cachep) {
	/* returns new slab for cache cachep */

	kmem_slab_t* slabp = (kmem_slab_t*)bmalloc(BLOCK_SIZE*cachep->slab_size);

	if (slabp == nullptr) return nullptr; // cachep->error = 1

	slabp->my_cache = cachep;
	slabp->my_colour = cachep->colour_next;
	cachep->colour_next = (++(cachep->colour_next) % cachep->colour_num);
	slabp->inuse = 0;
	slabp->free = 0;
	slabp->next_slab = nullptr;
	slabp->prev_slab = nullptr;

	/* cache is growing */
	cachep->growing = 1;

	/* block to slab mapping update */
	int block_num = ((int)slabp - start) / BLOCK_SIZE;
	for (int i = block_num; i < block_num + cachep->slab_size; i++) {
		block_to_slab_mapping[i] = slabp;
	}

	/* init array of indexes of free objects */
	for (int i = 0; i < cachep->objs_per_slab - 1; i++) {
		*(FREE_OBJS(slabp) + i) = i + 1;
	}
	*(FREE_OBJS(slabp) + cachep->objs_per_slab - 1) = -1;

	/* where do objects start */
	slabp->objs = (void*)((unsigned)(FREE_OBJS(slabp) + cachep->objs_per_slab)
		+ (slabp->my_colour)*CACHE_L1_LINE_SIZE);

	/* init all objects on the slab */
	for (int i = 0; i < cachep->objs_per_slab; i++) {
		if (cachep->ctor != nullptr) {
			(cachep->ctor)((void*)((unsigned)slabp->objs + i*cachep->obj_size));
		}
	}
	return slabp;
}

void* slab_alloc(kmem_slab_t* slabp) {
	if (slabp == nullptr) return nullptr;
	if (slabp->free == -1) return nullptr;
	void* objp = (void*)((int)slabp->objs + slabp->free*slabp->my_cache->obj_size);
	slabp->free = *(FREE_OBJS(slabp) + slabp->free);
	slabp->inuse++;
	return objp;
}

void slab_free(kmem_slab_t* slabp, void* objp) {
	if (slabp == nullptr) return;
	if (objp == nullptr) return;

	/* object must be on this slab */
	assert(is_obj_on_slab(slabp, objp));

	/* back to init state */
	if (slabp->my_cache->ctor != nullptr) {
		slabp->my_cache->ctor(objp);
	}

	unsigned objn = ((unsigned)objp - (unsigned)slabp->objs) / slabp->my_cache->obj_size;
	*(FREE_OBJS(slabp) + objn) = slabp->free;
	slabp->free = objn;
	slabp->inuse--;
}

int is_obj_on_slab(kmem_slab_t* slabp, void* objp) {
	if (slabp == nullptr || objp == nullptr) return 0;
	return (int)slabp <= (int)objp &&
		(int)objp <= ((int)slabp + (slabp->my_cache->slab_size*BLOCK_SIZE));
}

void add_empty_slab(kmem_cache_t* cachep) {
	/* for debugging purposes */

	if (cachep == nullptr) return;
	kmem_slab_t* slabp = new_slab(cachep);
	slab_add_to_list(&cachep->empty, slabp);
}

void cache_constructor(kmem_cache_t* cachep, const char* name, size_t size,
	void(*ctor)(void*),
	void(*dtor)(void*)) {

	strcpy(cachep->name, name);
	cachep->obj_size = size;
	cachep->ctor = ctor;
	cachep->dtor = dtor;
	cachep->growing = 0;
	cachep->num_of_slabs = 0;
	cachep->error = 0;
	cachep->num_of_active_objs = 0;

	/* if object size is larger then treshold slab desc. is kept off slab */
	cachep->off_slab = (size > OBJECT_TRESHOLD) ? 1:0;

	cachep->full = nullptr;
	cachep->partial = nullptr;
	cachep->empty = nullptr;

	/* init critical section of this cache */
	InitializeCriticalSection(&(cachep->cache_cs));

	/* puts new cache at the beginning of the cache list */
	cachep->next_cache = cache_head;
	cachep->prev_cache = nullptr;
	if (cache_head != nullptr) cache_head->prev_cache = cachep;
	cache_head = cachep;

	/* slab_size, objs_pre_slab, colour_num, colour_next */

	unsigned int pow = 0;
	unsigned int num = 1;

	while (!INSUFFICIENT_SLAB_SPACE(num + 1, pow, size)) num++;
	while (INSUFFICIENT_SLAB_SPACE(num, pow, size) || LEFT_OVER(num, pow, size)>(SLAB_SIZE(pow) >> 3)) {
		pow++;
		while (!INSUFFICIENT_SLAB_SPACE(num + 1, pow, size)) num++;
	}

	cachep->slab_size = (1 << pow);
	cachep->objs_per_slab = num;

	cachep->colour_next = 0;
	cachep->colour_num = LEFT_OVER(num, pow, size) / CACHE_L1_LINE_SIZE + 1;

#ifdef SLAB_DEBUG
	printf("left-over:%d 1/8:%d slab:%d\n", LEFT_OVER(num, pow, size), (SLAB_SIZE(pow) >> 3), sizeof(kmem_slab_t));
	printf("slab size:2^%d blocks, num:%d, colours:%d\n\n", pow, num, cachep->colour_num);
#endif
}

void cache_sizes_ctor(void* mem) {
	*(int*)mem = 0;
}

void cache_sizes_init() {
	/* initialize all size-N caches */
	kmem_cache_t* cachep = (kmem_cache_t*)bmalloc(CACHE_SIZES_NUM * sizeof(kmem_cache_t));

	/* this must not be nullptr */
	assert(cachep != nullptr);

	int pow = MIN_CACHE_SIZE;
	char name[CACHE_NAME_LEN];

	for (int i = 0; i < CACHE_SIZES_NUM; i++, pow++, cachep++) {

		unsigned int bsize = (1 << pow);
		sprintf_s(name, CACHE_NAME_LEN, "size-%d cache", bsize);
		cache_constructor(cachep, name, bsize, cache_sizes_ctor, nullptr);
		mem_buffer_array[i].cs_cachep = cachep;
	}
}

void enter_cs(kmem_cache_t* cachep) {
	EnterCriticalSection(&cachep->cache_cs);
}

void leave_cs(kmem_cache_t* cachep) {
	LeaveCriticalSection(&cachep->cache_cs);
}

/* ----------------------------------------------------------- */
/* -------------------------- CACHE -------------------------- */
/* ----------------------------------------------------------- */

kmem_cache_t *kmem_cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *)) {

	kmem_cache_t* my_cache = (kmem_cache_t*)kmalloc(sizeof(kmem_cache_t));
	if (my_cache == nullptr) return nullptr; 

	cache_constructor(my_cache, name, size, ctor, dtor);

	return my_cache;
}

void kmem_init(void *space, int block_num) {
	cache_head = nullptr;
	buddy_init(space, block_num);
	cache_sizes_init();
	start = (unsigned)space;
}

int kmem_cache_shrink(kmem_cache_t *cachep) {
	if (cachep == nullptr) return 0;

	/* ENTER CS */
	enter_cs(cachep);

	int num_of_freed_blocks = 0;

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

			/* block to slab mapping update */
			int block_num = ((int)slabp - start) / BLOCK_SIZE;
			for (int i = block_num; i < block_num + cachep->slab_size; i++) {
				block_to_slab_mapping[i] = nullptr;
			}

			bfree(slabp);
			cnt++;
		}

		num_of_freed_blocks = cachep->slab_size * cnt;
	}

	/* LEAVE CS */
	leave_cs(cachep);
	return num_of_freed_blocks;
}

void* kmem_cache_alloc(kmem_cache_t *cachep) {
	if (cachep == nullptr) return nullptr;

	/* ENTER CS */
	enter_cs(cachep);

	kmem_slab_t* slabp;
	void* objp = nullptr;

	if (cachep->partial == nullptr) {
		/* make new empty slab, allocate one obj and move it to partial */

		slabp = new_slab(cachep);
		if (slabp == nullptr) cachep->error = 1;
		else {
			cachep->num_of_slabs++;
			cachep->num_of_active_objs++;
			objp = slab_alloc(slabp);
			slab_add_to_list(&cachep->partial, slabp);
		}
	}
	else {
		/* allocate object from first partial slab */

		slabp = cachep->partial;
		objp = slab_alloc(slabp);
		if (slabp->free == -1) {
			/* if the slab is now full move it to full list */

			slab_remove_from_list(&cachep->partial, slabp);
			slab_add_to_list(&cachep->full, slabp);
		}
		cachep->num_of_active_objs++;
	}

	/* LEAVE CS */
	leave_cs(cachep);
	return objp;
}

void kmem_cache_free(kmem_cache_t *cachep, void *objp) {
	if (cachep == nullptr) return;
	if (objp == nullptr) return;

	/* ENTER CS */
	enter_cs(cachep);

	int block = ((((int)objp - start) & ~(BLOCK_SIZE - 1)) >> BLOCK_N);

	kmem_slab_t* slabp = block_to_slab_mapping[block];

	/* must not be nullptr */
	assert(slabp != nullptr);

	slab_free(slabp, objp);

	if (slabp->inuse == slabp->my_cache->objs_per_slab - 1) {
		/* move from full to partial */
		slab_remove_from_list(&slabp->my_cache->full, slabp);
		slab_add_to_list(&slabp->my_cache->partial, slabp);
	}
	else if (slabp->inuse == 0) {
		/* move from partial to empty */
		slab_remove_from_list(&slabp->my_cache->partial, slabp);
		slab_add_to_list(&slabp->my_cache->empty, slabp);
	}

	cachep->num_of_active_objs--;

	/* LEAVE CS */
	leave_cs(cachep);

	/* first leave CS to avoid deadlock */
	kmem_cache_shrink(cachep);

	return;
}

void kmem_cache_destroy(kmem_cache_t *cachep) {
	/* does not have CS */

	if (cachep == nullptr) return;
	if (cachep->full != nullptr || cachep->partial != nullptr) return;
	cache_remove_from_list(cachep);
	cachep->growing = 0;
	kmem_cache_shrink(cachep);
	DeleteCriticalSection(&cachep->cache_cs);
	kfree(cachep);

}

void kmem_cache_info(kmem_cache_t* cachep) {

	/* ENTER CS */
	enter_cs(cachep);

	int empty_slab_cnt = 0;
	kmem_slab_t* slabp = cachep->empty;
	while (slabp != nullptr) {
		slabp = slabp->next_slab;
		empty_slab_cnt++;
	}
	
	int active_objs = cachep->num_of_active_objs;
	int total_objs = cachep->num_of_slabs * cachep->objs_per_slab;
	int obj_size = cachep->obj_size;
	int active_slabs = cachep->num_of_slabs - empty_slab_cnt;
	int total_slabs = cachep->num_of_slabs;
	int pages_per_slab = cachep->slab_size;

	printf("\n%-.*s%7d%7d%7d%5d%5d%5d\n", CACHE_NAME_LEN, 
										cachep->name, 
										active_objs, 
										total_objs,
										obj_size,
										active_slabs,
										total_slabs,
										pages_per_slab 
		  );

	/* LEAVE CS */
	leave_cs(cachep);
}

int kmem_cache_error(kmem_cache_t *cachep) {
	if (cachep == nullptr) return -1;
	return cachep->error;
}

void* kmalloc(size_t size) {
	int pow = MIN_CACHE_SIZE;
	while ((1 << pow) < size) pow++;
	return kmem_cache_alloc(mem_buffer_array[pow - MIN_CACHE_SIZE].cs_cachep);
}

void kfree(const void *objp) {
	if (objp == nullptr) return;

	int block = ((((int)objp - start) & ~(BLOCK_SIZE - 1)) >> BLOCK_N);
	kmem_slab_t* slabp = block_to_slab_mapping[block];

	/* deadlock free */
	kmem_cache_free(slabp->my_cache, (void*)objp);
	kmem_cache_shrink(slabp->my_cache);
}
	

