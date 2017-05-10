#include "slab.h"
#include <string.h>
#include <assert.h>
#include <mutex>

/* for size-N caches mem_buffer_array must follow those values! */
#define CACHE_SIZES_NUM (13)
#define MIN_CACHE_SIZE (5)

//#define LINUX_LIKE_CACHE_INFO

/* ---------------------------------------------------------- */
/* ------------------------- STRUCTS ------------------------ */
/* ---------------------------------------------------------- */

typedef struct size_N {
	size_t cs_size;
	kmem_cache_t* cs_cachep;
} size_N_t;

typedef struct kmem_slab_s {
	struct kmem_slab_s* next_slab; // initially nullptr
	struct kmem_slab_s* prev_slab; // initially nullptr
	struct kmem_cache_s* my_cache;
	unsigned int my_colour;        // offset
	unsigned int inuse;            // number of used objects 
	unsigned int free;             // index of first free object 
	void* objs;                    // pointer to first object 
}kmem_slab_t;

typedef struct kmem_cache_s {
	struct kmem_cache_s* next_cache; // initially nullptr
	struct kmem_cache_s* prev_cache; // initially nullptr

	kmem_slab_t* full;               // initially nullptr
	kmem_slab_t* partial;            // initially nullptr
	kmem_slab_t* empty;              // initially nullptr

	size_t obj_size; 

	/* mutex is shared between processes */
	void* mutex_placement;
	std::mutex* cache_mutex;

	unsigned int slab_size;
	unsigned int num_of_slabs; 
	unsigned int objs_per_slab;
	unsigned int colour_num;
	unsigned int colour_next;
	unsigned int num_of_active_objs;
	int off_slab;

	/* set it to 1 when cache is expanded, set it to 0 when cache is shrinked */
	unsigned int growing;

	/* set to 1 when destroy is called on cache with active objects          */
	/* set to 1 when cache can't allocate more memory for it's objects       */
	unsigned int error;

	void(*ctor)(void*); 
	void(*dtor)(void*); 
	char name[CACHE_NAME_LEN];

} kmem_cache_t;

/* ---------------------------------------------------------- */
/* ------------------------- GLOBALS ------------------------ */
/* ---------------------------------------------------------- */

/* cache used to store kmem_cache_t structs */
static kmem_cache_t cache_cache;

/* cache used to store std::mutex structs */
static kmem_cache_t mutex_cache;

/* beginning of available space */
static unsigned start;

/* head of cache linked list */
static kmem_cache_t* cache_head;

/* mutexes for static caches */
static std::mutex mutex_cache_mutex;
static std::mutex cache_cache_mutex;
static std::mutex size_N_mutex[CACHE_SIZES_NUM];

/* all pointers are set to nullptr at the beginning */
static kmem_slab_t** block_to_slab_mapping;

/* static array of size-N caches */
static size_N_t size_N_caches[] = {
	{ 32, nullptr },     // 5
	{ 64, nullptr },     // 6
	{ 128, nullptr },    // 7
	{ 256, nullptr },    // 8
	{ 512, nullptr },    // 9
	{ 1024, nullptr },   // 10
	{ 2048, nullptr },   // 11
	{ 4096, nullptr },   // 12
	{ 8192, nullptr },   // 13
	{ 16384, nullptr },  // 14
	{ 32768, nullptr },  // 15
	{ 65536, nullptr },  // 16
	{ 131072, nullptr }  // 17
};

int block_N;

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
	/* for debugging purposes */

	if (slabp == nullptr) return;
	printf("\nallocated %d blocks\n", slabp->my_cache->slab_size);
	printf("slab desc. start %d\n", (int)slabp - (int)slabp);
	printf("slab desc. end %d\n", (int)slabp - (int)slabp + sizeof(kmem_slab_t));
	printf("slab desc. array start %d\n", (unsigned)FREE_OBJS(slabp) - (unsigned)slabp);

	for (int i = 0; i < slabp->my_cache->objs_per_slab; i++) {
		printf("%d ", *(FREE_OBJS(slabp) + i));
	}
	printf("\n");
	printf("slab desc. array end %d\n", (unsigned)FREE_OBJS(slabp) - (unsigned)slabp + 4 * slabp->my_cache->objs_per_slab);
	if (slabp->my_cache->off_slab == 1) {
		printf("slab desc.off slab\n");
		printf("objs slab start %d\n", 0);

		for (int i = 0; i < slabp->my_cache->objs_per_slab; i++) {
			printf("%d - %d\n", 
				((unsigned)slabp->objs + i*slabp->my_cache->obj_size) - (unsigned)slabp->objs + (slabp->my_colour)*CACHE_L1_LINE_SIZE,
				*(unsigned*)((unsigned)slabp->objs + i*slabp->my_cache->obj_size));
		}
		printf("slab objs end %d\n", ((unsigned)slabp->objs + slabp->my_cache->objs_per_slab*slabp->my_cache->obj_size)
			- (unsigned)slabp->objs + (slabp->my_colour)*CACHE_L1_LINE_SIZE);

		printf("slab end %d\n", BLOCK_SIZE*(slabp->my_cache->slab_size));
	}
	else {
		printf("slab desc. on slab\n");
		printf("slab objs start %d\n", (unsigned)slabp->objs - (unsigned)slabp);

		for (int i = 0; i < slabp->my_cache->objs_per_slab; i++) {
			printf("%d - %d\n", ((unsigned)slabp->objs + i*slabp->my_cache->obj_size) - (unsigned)slabp,
				*(unsigned*)((unsigned)slabp->objs + i*slabp->my_cache->obj_size));
		}
		printf("slab objs end %d\n", ((unsigned)slabp->objs + slabp->my_cache->objs_per_slab*slabp->my_cache->obj_size)
			- (unsigned)slabp);

		printf("slab end %d\n", BLOCK_SIZE*(slabp->my_cache->slab_size));
	}
}

kmem_slab_t* new_slab(kmem_cache_t* cachep) {
	/* returns new slab for cache cachep */

	/* Inside cachep CS */

	if (cachep == nullptr) return nullptr;

	kmem_slab_t* slabp;

	if (cachep->off_slab == 1) {
		/* if slab descriptor is kept off slab */

		slabp = (kmem_slab_t*)kmalloc(sizeof(kmem_slab_t) + (cachep->objs_per_slab)*sizeof(int));
		if (slabp == nullptr) return nullptr;

		slabp->objs = bmalloc(BLOCK_SIZE*cachep->slab_size);
		if (slabp->objs == nullptr) return nullptr;

		/* coulouring */
		slabp->objs = (void*)((unsigned)slabp->objs + (cachep->colour_next)*CACHE_L1_LINE_SIZE);
	}
	else {
		/* if slab descriptor is kept on slab */

		slabp = (kmem_slab_t*)bmalloc(BLOCK_SIZE*cachep->slab_size);
		if (slabp == nullptr) return nullptr;

		/* coulouring */
		slabp = (kmem_slab_t*)((unsigned)slabp + (cachep->colour_next)*CACHE_L1_LINE_SIZE);
		slabp->objs = (void*)(unsigned)(FREE_OBJS(slabp) + cachep->objs_per_slab);
	}

	slabp->my_colour = cachep->colour_next;
	cachep->colour_next = (++(cachep->colour_next) % cachep->colour_num);
	slabp->my_cache = cachep;
	slabp->inuse = 0;
	slabp->free = 0;
	slabp->next_slab = nullptr;
	slabp->prev_slab = nullptr;

	/* cache is growing */
	cachep->growing = 1;

	/* init array of indexes of free objects (always kept on slab)*/
	for (int i = 0; i < cachep->objs_per_slab - 1; i++) {
		FREE_OBJS(slabp)[i] = i + 1;
	}
	FREE_OBJS(slabp)[cachep->objs_per_slab - 1] = -1;

	/* init all objects on the slab */
	process_objects_on_slab(slabp, cachep->ctor);

	/* block to slab mapping update */
	btsm_update(slabp, slabp);

	return slabp;
}

void* slab_alloc(kmem_slab_t* slabp) {
	if (slabp == nullptr) return nullptr;
	if (slabp->free == -1) return nullptr;
	void* objp = (void*)((int)slabp->objs + slabp->free*slabp->my_cache->obj_size);
	slabp->free = FREE_OBJS(slabp)[slabp->free];
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

	unsigned objn = ((int)objp - (int)slabp->objs) / slabp->my_cache->obj_size;
	FREE_OBJS(slabp)[objn] = slabp->free;
	slabp->free = objn;
	slabp->inuse--;
}

int is_obj_on_slab(kmem_slab_t* slabp, void* objp) {
	if (slabp == nullptr || objp == nullptr) return 0;
	return ((int)slabp->objs <= (int)objp &&
		(int)objp <= ((int)slabp->objs + (slabp->my_cache->slab_size*BLOCK_SIZE)));
}

void add_empty_slab(kmem_cache_t* cachep) {
	/* for debugging purposes */

	if (cachep == nullptr) return;
	kmem_slab_t* slabp = new_slab(cachep);
	slab_add_to_list(&cachep->empty, slabp);
	kmem_slab_info(slabp);
}

void kmem_cache_constructor(kmem_cache_t* cachep, const char* name, size_t size,
	void(*ctor)(void*),
	void(*dtor)(void*)) {
	/* Does NOT initiazlize mutex_placement & cache_mutex */

	strcpy(cachep->name, name);
	cachep->obj_size = size;
	cachep->ctor = ctor;
	cachep->dtor = dtor;
	cachep->growing = 0;
	cachep->num_of_slabs = 0;
	cachep->error = 0;
	cachep->num_of_active_objs = 0;

	/* if object size is larger then treshold slab desc. is kept off slab */
	cachep->off_slab = ((size > OBJECT_TRESHOLD) ? 1 : 0);

	cachep->full = nullptr;
	cachep->partial = nullptr;
	cachep->empty = nullptr;

	/* puts new cache at the beginning of the cache list */
	cachep->next_cache = cache_head;
	cachep->prev_cache = nullptr;
	if (cache_head != nullptr) cache_head->prev_cache = cachep;
	cache_head = cachep;

	/* slab_size, objs_pre_slab, colour_num, colour_next */

	unsigned int pow = 0;
	unsigned int num = 1;

	kmem_cache_estimate(&pow, &num, size, cachep->off_slab); 

	cachep->slab_size = (1 << pow);
	cachep->objs_per_slab = num;

	cachep->colour_next = 0;
	cachep->colour_num = LEFT_OVER(num, pow, size, cachep->off_slab) / CACHE_L1_LINE_SIZE + 1; 
}

void kmem_cache_estimate(unsigned* pow, unsigned* num, int size, int off) {
	/* memory wastage is less then 1/8 of total slab size */

	*pow = 0;
	*num = 1;

	while (!INSUFFICIENT_SLAB_SPACE((*num) + 1, *pow, size, off)) (*num)++;
	while (INSUFFICIENT_SLAB_SPACE(*num, *pow, size, off) || 
		   LEFT_OVER(*num, *pow, size, off)>(SLAB_SIZE(*pow) >> 3)) {
		(*pow)++;
		while (!INSUFFICIENT_SLAB_SPACE((*num) + 1, *pow, size, off)) (*num)++;
	}
}

void cache_ctor(void* mem) {
	*(int*)mem = 0;
}

void static_caches_init() {

	/* init cache_cache with static mutex */
	cache_cache.cache_mutex = &cache_cache_mutex;
	cache_cache.mutex_placement = nullptr;
	kmem_cache_constructor(&cache_cache, "cache-cache\0", sizeof(kmem_cache_t), cache_ctor, nullptr);

	/* init mutex_cache with static mutex */
	mutex_cache.cache_mutex = &mutex_cache_mutex;
	mutex_cache.mutex_placement = nullptr;
	kmem_cache_constructor(&mutex_cache, "mutex-cache\0", sizeof(std::mutex), cache_ctor, nullptr);

	int pow = MIN_CACHE_SIZE;
	char name[CACHE_NAME_LEN];

	/* init all size-N caches */
	for (int i = 0; i < CACHE_SIZES_NUM; i++) {

		kmem_cache_t* cachep = (kmem_cache_t*)kmem_cache_alloc(&cache_cache);
		
		unsigned int bsize = (1 << pow);
		sprintf_s(name, CACHE_NAME_LEN, "size-%d cache", bsize);

		/* init size-N cache with static mutex */
		cachep->cache_mutex = &(size_N_mutex[i]);
		cachep->mutex_placement = nullptr;

		kmem_cache_constructor(cachep, name, bsize, cache_ctor, nullptr);
		size_N_caches[i].cs_cachep = cachep;

		pow++;
	}
}

void enter_cs(kmem_cache_t* cachep) {
	cachep->cache_mutex->lock();
}

void leave_cs(kmem_cache_t* cachep) {
	cachep->cache_mutex->unlock();
}

void process_objects_on_slab(kmem_slab_t* slabp, void(*function)(void *)) {
	if (slabp == nullptr || function == nullptr) return;
	int obj_num = slabp->my_cache->objs_per_slab;
	int obj_size = slabp->my_cache->obj_size;

	for (int i = 0; i < obj_num; i++) {
			function((void*)((int)slabp->objs + i*obj_size));
	}
}

void btsm_update(kmem_slab_t* slabp, kmem_slab_t* set_to) {
	if (slabp == nullptr) return;
	int blockn = ((((int)slabp->objs - start) & ~(BLOCK_SIZE - 1)) >> block_N);
	int limit = blockn + slabp->my_cache->slab_size;

	for (int i = blockn; i < limit; i++) {
		block_to_slab_mapping[i] = set_to;
	}
}

int kmem_cache_check_name_availability(const char* name) {
	/* Returns 0 if name is unavailable */

	if (name == nullptr) return 0;
	for (kmem_cache_t* i = cache_head; i != nullptr; i = i->next_cache) {
		if (strcmp(name, i->name) == 0) return 0;
	}
	return 1;
}

int kmem_cache_shrink_no_cs(kmem_cache_t *cachep) {
	/* Does not have critial section */

	if (cachep == nullptr) return 0;

	if (cachep->empty != nullptr && cachep->growing == 1) {
		cachep->growing = 0;
		return 0;
	}

	int num_of_freed_blocks = 0;

	while (cachep->empty != nullptr) {
		/* free all empty slabs */

		kmem_slab_t* slabp = slab_remove_from_list(&cachep->empty, cachep->empty);
		cachep->num_of_slabs--;

		/*              --- block to slab mapping update ---                     */

		/* !!!  if used, MUST be before bfree to avoid data corruption:  !!!     */

		/* - POSSIBLE SCENARIO (btsm_update after bfree):                        */
		/* - 1) bfree is called and blocks are freed.                            */
		/* - 2) some other thread allocate those blocks for cache [C],           */
		/*      and sets pointers to its slab.                                   */
		/* - 3) btsm_update is called and those pointers are set to nullptr,     */
		/*      where they should point to slab of cache [C].                    */
		/* - 4) cache [C] is left with corrupted pointers to its slab.           */
		/* - 5) assertion in kmem_cache_free() will be triggered.                */

		//btsm_update(slabp, nullptr);

		/* destroy all objects on this slab */
		process_objects_on_slab(slabp, cachep->dtor);

		if (cachep->off_slab == 1) {
			/* if slab descriptor is kept off slab */

			bfree(slabp->objs);
			kfree(slabp);
		}
		else bfree(slabp);

		num_of_freed_blocks += cachep->slab_size;
	}

	return num_of_freed_blocks;
}

/* ----------------------------------------------------------- */
/* -------------------------- CACHE -------------------------- */
/* ----------------------------------------------------------- */

kmem_cache_t *kmem_cache_create(const char *name, size_t size,
	void(*ctor)(void *),
	void(*dtor)(void *)) {

	if (kmem_cache_check_name_availability(name) == 0) return nullptr;
	
	kmem_cache_t* cachep = (kmem_cache_t*)kmem_cache_alloc(&cache_cache);
	if (cachep == nullptr) return nullptr; 

	/* placement new operator does not allocate memory */
	cachep->mutex_placement = kmem_cache_alloc(&mutex_cache);
	cachep->cache_mutex = new std::mutex(), cachep->mutex_placement;

	kmem_cache_constructor(cachep, name, size, ctor, dtor);

	return cachep;
}

void kmem_init(void *space, int block_num) {

	/* assert MUST be true, else program will crash during cache_cache's         */
	/* first slab allocation(for size-32 cache) since it needs kmalloc           */
	/* for slab descriptor and it*s making size-N caches which provide kmalloc.  */
	
	assert(sizeof(kmem_cache_t)<OBJECT_TRESHOLD);

	int block_is_lost = 0;

	/* align space with BLOCK_SIZE multiple and see if block is lost */
	if ((unsigned(space) & (BLOCK_SIZE - 1)) > 0) {
		block_is_lost = 1;
	}
	space = (void*)(((unsigned)space + BLOCK_SIZE-1) & ~(BLOCK_SIZE - 1));

	cache_head = nullptr;
	block_N = 0;
	while ((1 << block_N) < BLOCK_SIZE) block_N++;

	int buddy_num_of_blocks = block_num - block_is_lost;

	/* aligned space is given to buddy_init */
	space = buddy_init(space, &buddy_num_of_blocks);

	start = (unsigned)space;

	block_to_slab_mapping = (kmem_slab_t**)bmalloc(sizeof(kmem_slab_t*)*buddy_num_of_blocks);

	for (int i = 0; i < buddy_num_of_blocks; i++) {
		block_to_slab_mapping[i] = nullptr;
	}

	static_caches_init();
}

int kmem_cache_shrink(kmem_cache_t *cachep) {
	if (cachep == nullptr) return 0;

	/* ENTER CS */
	enter_cs(cachep);

	int num_of_freed_blocks = kmem_cache_shrink_no_cs(cachep);

	/* LEAVE CS */
	leave_cs(cachep);
	return num_of_freed_blocks;
}

void* kmem_cache_alloc(kmem_cache_t *cachep) {
	if (cachep == nullptr) return nullptr;

	/* ENTER CS */
	enter_cs(cachep);

	kmem_slab_t* slabp = nullptr;
	void* objp = nullptr;

	if (cachep->partial != nullptr) slabp = cachep->partial;
	else if (cachep->empty == nullptr) {
		/* partial == nullptr && empty == nullptr */

		slabp = new_slab(cachep);
		slab_add_to_list(&cachep->partial, slabp);
		cachep->num_of_slabs++;
	}
	else{
		/* partial == nullptr && empty != nullptr  */

		slabp = cachep->empty;
		assert(slabp->inuse == 0);
		slab_remove_from_list(&cachep->empty, slabp);
		slab_add_to_list(&cachep->partial, slabp);
	}

	/* use if program crashes while trying to reference nullptr */
	/* to prove that buddy ran out of blocks.                   */
	//assert(slabp != nullptr);

	if (slabp == nullptr) cachep->error = 1;
	else {
		/* slabp is in partial */

		objp = slab_alloc(slabp);
		if (slabp->free == -1) {
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

	int blockn = ((((int)objp - start) & ~(BLOCK_SIZE - 1)) >> block_N);

	kmem_slab_t* slabp = block_to_slab_mapping[blockn];

	/* must not be nullptr */
	assert(slabp != nullptr);

	slab_free(slabp, objp);

	if (slabp->inuse == 0) {
		/* move from full/partial to empty */
		if (slabp->my_cache->objs_per_slab == 1) 
			slab_remove_from_list(&slabp->my_cache->full, slabp);
		else slab_remove_from_list(&slabp->my_cache->partial, slabp);

		slab_add_to_list(&slabp->my_cache->empty, slabp);

		/* try to shrink cache */
		kmem_cache_shrink_no_cs(cachep);
	}
	else if (slabp->inuse == (slabp->my_cache->objs_per_slab - 1)) {
		/* move from full to partial */
		slab_remove_from_list(&slabp->my_cache->full, slabp);
		slab_add_to_list(&slabp->my_cache->partial, slabp);
	}
	
	cachep->num_of_active_objs--;

	/* LEAVE CS */
	leave_cs(cachep);

	return;
}

void kmem_cache_destroy(kmem_cache_t *cachep) {
	/* does not have CS */

	if (cachep == nullptr) return;
	if (cachep->full != nullptr || cachep->partial != nullptr) {
		/* cache that is about to be destroyed must not have active objects on it */

		cachep->error = 1;
		return;
	}
	cache_remove_from_list(cachep);
	cachep->growing = 0;
	kmem_cache_shrink(cachep);
	kmem_cache_free(&mutex_cache, cachep->mutex_placement);
	kmem_cache_free(&cache_cache, cachep);
}

void kmem_cache_info(kmem_cache_t* cachep) {

	/* ENTER CS */
	enter_cs(cachep);

#ifdef LINUX_LIKE_CACHE_INFO
	int empty_slab_cnt = 0;
	kmem_slab_t* slabp = cachep->empty;
	while (slabp != nullptr) {
		slabp = slabp->next_slab;
		empty_slab_cnt++;
	}

	int active_slabs = cachep->num_of_slabs - empty_slab_cnt;
#endif

	int active_objs = cachep->num_of_active_objs;
	int total_objs = cachep->num_of_slabs * cachep->objs_per_slab;
	int total_slabs = cachep->num_of_slabs;
	int blocks_per_slab = cachep->slab_size;

	int obj_size = cachep->obj_size;

	/* in size of blocks */
	int total_cache_size = cachep->num_of_slabs*cachep->slab_size;

	int num_of_slabs = cachep->num_of_slabs;
	int objs_per_slab = cachep->objs_per_slab;
	double percentage_used;

	/* percentage is -1 when cache has no slabs */
	if (total_objs != 0) percentage_used = (double)active_objs / total_objs * 100;
	else percentage_used = -1;

#ifdef LINUX_LIKE_CACHE_INFO
	printf("%-*s%*d %*d %*d %*d %*d %*d\n", CACHE_NAME_LEN, cachep->name,
											7, active_objs, 
											7, total_objs,
											7, obj_size,
											5, active_slabs,
											5, total_slabs,
											5, blocks_per_slab 
		  );
#else
	printf("%-*s%*d %*d %*d %*d %*.2f%%\n", CACHE_NAME_LEN, cachep->name,
											5, obj_size,
											7, total_cache_size,
											7, num_of_slabs,
											5, objs_per_slab,
											7, percentage_used
	);
#endif

	/* LEAVE CS */
	leave_cs(cachep);
}

int kmem_cache_error(kmem_cache_t *cachep) {
	if (cachep == nullptr) return -1;

	/* ENTER CS */
	enter_cs(cachep);

	int err = cachep->error;

	/* LEAVE CS */
	leave_cs(cachep);

	return err;
}

void* kmalloc(size_t size) {
	int pow = MIN_CACHE_SIZE;
	while ((1 << pow) < size) pow++;

	void* objp = kmem_cache_alloc(size_N_caches[pow - MIN_CACHE_SIZE].cs_cachep);

	return objp;
}

void kfree(const void *objp) {
	if (objp == nullptr) return;

	int blockn = ((((int)objp - start) & ~(BLOCK_SIZE - 1)) >> block_N);
	kmem_slab_t* slabp = block_to_slab_mapping[blockn];

	assert(slabp != nullptr);

	kmem_cache_free(slabp->my_cache, (void*)objp);
}