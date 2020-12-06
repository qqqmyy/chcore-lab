#include <common/util.h>
#include <common/macro.h>
#include <common/kprint.h>

#include "buddy.h"

/*
 * The layout of a phys_mem_pool:
 * | page_metadata are (an array of struct page) | alignment pad | usable memory |
 *
 * The usable memory: [pool_start_addr, pool_start_addr + pool_mem_size).
 */
void init_buddy(struct phys_mem_pool *pool, struct page *start_page,
		vaddr_t start_addr, u64 page_num)
{
	int order;
	int page_idx;
	struct page *page;

	/* Init the physical memory pool. */
	pool->pool_start_addr = start_addr;
	pool->page_metadata = start_page;
	pool->pool_mem_size = page_num * BUDDY_PAGE_SIZE;
	/* This field is for unit test only. */
	pool->pool_phys_page_num = page_num;

	/* Init the free lists */
	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		pool->free_lists[order].nr_free = 0;
		init_list_head(&(pool->free_lists[order].free_list));
	}

	/* Clear the page_metadata area. */
	memset((char *)start_page, 0, page_num * sizeof(struct page));

	/* Init the page_metadata area. */
	for (page_idx = 0; page_idx < page_num; ++page_idx) {
		page = start_page + page_idx;
		page->allocated = 1;
		page->order = 0;
	}

	/* Put each physical memory page into the free lists. */
	for (page_idx = 0; page_idx < page_num; ++page_idx) {
		page = start_page + page_idx;
		buddy_free_pages(pool, page);
	}
}

static struct page *get_buddy_chunk(struct phys_mem_pool *pool,
				    struct page *chunk)
{
	u64 chunk_addr;
	u64 buddy_chunk_addr;
	int order;

	/* Get the address of the chunk. */
	chunk_addr = (u64) page_to_virt(pool, chunk);
	order = chunk->order;
	/*
	 * Calculate the address of the buddy chunk according to the address
	 * relationship between buddies.
	 */
#define BUDDY_PAGE_SIZE_ORDER (12)
	buddy_chunk_addr = chunk_addr ^
	    (1UL << (order + BUDDY_PAGE_SIZE_ORDER));

	/* Check whether the buddy_chunk_addr belongs to pool. */
	if ((buddy_chunk_addr < pool->pool_start_addr) ||
	    (buddy_chunk_addr >= (pool->pool_start_addr +
				  pool->pool_mem_size))) {
		return NULL;
	}

	return virt_to_page(pool, (void *)buddy_chunk_addr);
}

/*
 * split_page: split the memory block into two smaller sub-block, whose order
 * is half of the origin page.
 * pool @ physical memory structure reserved in the kernel
 * order @ order for origin page block
 * page @ splitted page
 * 
 * Hints: don't forget to substract the free page number for the corresponding free_list.
 * you can invoke split_page recursively until the given page can not be splitted into two
 * smaller sub-pages.
 */

static struct page *split_page(struct phys_mem_pool *pool, u64 order,
			       struct page *page)
{
	// <lab2>

	if (page->order <= order || page->allocated == 1)
	{
		return page;
	}
	if (page->order - order > 1)
	{
		page = split_page(pool,order + 1,page);
	}
	// else
	// {
		struct free_list *origin_free_list = &(pool->free_lists[page->order]);
		struct free_list *new_free_list = &(pool->free_lists[page->order - 1]);

		page->order--;
		origin_free_list->nr_free--;
		list_del(&page->node);
		
		struct page *buddy_page = get_buddy_chunk(pool,page);
		buddy_page->allocated = 0;
		buddy_page->order = page->order;

		new_free_list->nr_free = new_free_list->nr_free + 2;
		list_add(&page->node,&new_free_list->free_list);
		list_add(&buddy_page->node,&new_free_list->free_list);
	// }
	
	return page;
	// </lab2>
}


struct page *buddy_get_pages(struct phys_mem_pool *pool, u64 order)
{
	// <lab2>

	if (order >= BUDDY_MAX_ORDER)
	{
		return NULL;
	}
	
	struct page *page = NULL;

	struct free_list *order_free_list = &(pool->free_lists[order]);
	if (order_free_list->nr_free != 0)
	{
		struct list_head *list_node = order_free_list->free_list.next;
		page = list_entry(list_node,struct page,node);
		page->allocated = 1;
		list_del(&page->node);
		order_free_list->nr_free--;
	}
	else
	{
		u64 new_order = order;
		while (order_free_list->nr_free == 0)
		{
			new_order++;
			if (new_order >= BUDDY_MAX_ORDER)
			{
				return NULL;
			}
			order_free_list = &(pool->free_lists[new_order]);
		}
		struct list_head *to_split_page_node = order_free_list->free_list.next;
		struct page *to_split_page = list_entry(to_split_page_node,struct page,node);
		page = split_page(pool,order,to_split_page);

		page->allocated = 1;
		list_del(to_split_page_node);
		pool->free_lists[order].nr_free--;

	}
	return page;



	// </lab2>
}



/*
 * merge_page: merge the given page with the buddy page
 * pool @ physical memory structure reserved in the kernel
 * page @ merged page (attempted)
 * 
 * Hints: you can invoke the merge_page recursively until
 * there is not corresponding buddy page. get_buddy_chunk
 * is helpful in this function.
 */
static struct page *merge_page(struct phys_mem_pool *pool, struct page *page)
{
	// <lab2>
	// Page cannot be merged
	if (page->order == BUDDY_MAX_ORDER - 1) {
		return page;
	}

	int page_idx = page - pool->page_metadata;
	struct page *buddy_page = get_buddy_chunk(pool, page);
	// Check if the buddy page exists and can be merged
	if (buddy_page == NULL || buddy_page->order != page->order || buddy_page->allocated) {
		return page;
	}

	// Remove the buddy page from the free list
	--pool->free_lists[buddy_page->order].nr_free;
	list_del(&(buddy_page->node));

	// Remove self from the free list
	--pool->free_lists[page->order].nr_free;
	list_del(&(page->node));

	// Merge two pages
	int merge_page_order = page->order + 1;

	// Align the address
	u64 merge_page_idx = page_idx & ~((1 << merge_page_order) - 1);
	struct page *merged = merge_page_idx + pool->page_metadata;
	merged->allocated = 0;
	merged->order = merge_page_order;

	// Insert into free list
	++pool->free_lists[merged->order].nr_free;
	list_append(&(merged->node), &(pool->free_lists[merged->order].free_list));
	return merge_page(pool, merged);
	// </lab2>
}

/*
 * buddy_free_pages: give back the pages to buddy system
 * pool @ physical memory structure reserved in the kernel
 * page @ free page structure
 * 
 * Hints: you can invoke merge_page.
 */

// /*
//  * buddy_free_pages: give back the pages to buddy system
//  * pool @ physical memory structure reserved in the kernel
//  * page @ free page structure
//  * 
//  * Hints: you can invoke merge_page.
//  */
void buddy_free_pages(struct phys_mem_pool *pool, struct page *page)
{
	// <lab2>
	// Deallocate the page
	page->allocated = 0;
	// Insert into free list
	++pool->free_lists[page->order].nr_free;
	list_append(&(page->node), &(pool->free_lists[page->order].free_list));
	// Try merge two page
	merge_page(pool, page);
	// </lab2>
}

void *page_to_virt(struct phys_mem_pool *pool, struct page *page)
{
	u64 addr;

	/* page_idx * BUDDY_PAGE_SIZE + start_addr */
	addr = (page - pool->page_metadata) * BUDDY_PAGE_SIZE +
	    pool->pool_start_addr;
	return (void *)addr;
}

struct page *virt_to_page(struct phys_mem_pool *pool, void *addr)
{
	struct page *page;

	page = pool->page_metadata +
	    (((u64) addr - pool->pool_start_addr) / BUDDY_PAGE_SIZE);
	return page;
}

u64 get_free_mem_size_from_buddy(struct phys_mem_pool * pool)
{
	int order;
	struct free_list *list;
	u64 current_order_size;
	u64 total_size = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; order++) {
		/* 2^order * 4K */
		current_order_size = BUDDY_PAGE_SIZE * (1 << order);
		list = pool->free_lists + order;
		total_size += list->nr_free * current_order_size;

		/* debug : print info about current order */
		kdebug("buddy memory chunk order: %d, size: 0x%lx, num: %d\n",
		       order, current_order_size, list->nr_free);
	}
	return total_size;
}
