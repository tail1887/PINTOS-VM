/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include <debug.h>
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "threads/thread.h"


/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);
static uint64_t page_hash (const struct hash_elem *e, void *aux);
static bool page_less (const struct hash_elem *a,
		const struct hash_elem *b, void *aux);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	void *va = pg_round_down(upage);

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, va) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page;
		bool (*initializer)(struct page *, enum vm_type, void *);

		page = malloc(sizeof *page);
		if(page == NULL)
			return false;

		if (VM_TYPE(type) == VM_ANON)
			initializer = anon_initializer;
		else if	(VM_TYPE(type) == VM_FILE)
			initializer = file_backed_initializer;
		else{
			free(page);
			return false;
		}

		uninit_new(page, va, init, type, aux, initializer);
		
		page->writable = writable;

		/* TODO: Insert the page into the spt. */
		if(spt_insert_page(spt, page))
			return true;
		else{
			free(page);
			return false;
		}
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	va = pg_round_down (va);

	struct page temp;
	temp.va = va;

	struct hash_elem *e = hash_find (&spt->hash, &temp.elem);
	if (e == NULL) {
		return NULL;
	}
	return hash_entry (e, struct page, elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	if (pg_ofs (page->va) != 0)
		return false;

	return hash_insert (&spt->hash, &page->elem) == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete (&spt->hash, &page->elem);
	vm_dealloc_page (page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {

	struct frame *frame = NULL;
	frame = malloc(sizeof(*frame));
	ASSERT(frame != NULL);
	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER);
	if(frame->kva == NULL) {
		free(frame);
		frame = vm_evict_frame();
	}

	ASSERT (frame != NULL);
	frame->page = NULL;
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct supplemental_page_table *spt = &thread_current()->spt;
	page = spt_find_page(spt, va);
	
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	assert(frame != NULL);
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	void * upage = page->va;
	void * kpage = pg_round_down(frame->kva);

	assert(pml4_set_page(thread_current()->pml4, upage, kpage, page->writable));
	
	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	if (!hash_init (&spt->hash, page_hash, page_less, NULL))
		PANIC ("supplemental_page_table_init: hash_init failed");
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* spt kill용 페이지 제거기 */ 
static void
spt_page_destructor(struct hash_elem *e, void *aux) {
	struct page *p = hash_entry(e, struct page, elem);
	vm_dealloc_page(p);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(&(spt->hash), spt_page_destructor);

}

/* Returns a hash value for a page based on its user virtual address. */
static uint64_t
page_hash (const struct hash_elem *e, void *aux UNUSED) {
	/* 이 경로로는 쓰기가 불가능하다는 걸 표시하기 위해 const를 사용. */
	const struct page *page = hash_entry (e, struct page, elem);
	return hash_bytes (&page->va, sizeof page->va);
}

/* Orders pages by user virtual address inside the SPT hash buckets. */
static bool
page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	const struct page *page_a = hash_entry (a, struct page, elem);
	const struct page *page_b = hash_entry (b, struct page, elem);
	return page_a->va < page_b->va;
}
