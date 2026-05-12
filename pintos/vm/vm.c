/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "lib/kernel/hash.h"
#include "vm/anon.h"
#include "vm/file.h"
#include "threads/mmu.h"
#include "threads/list.h"
#include <string.h>
#include "threads/vaddr.h"
#include <stdint.h>


static struct list frame_table;
static bool frame_table_initialized;

static void
frame_table_init (void)
{
	if (!frame_table_initialized) {
		list_init (&frame_table);
		frame_table_initialized = true;
	}
}

void
frame_table_add (struct frame *frame)
{
	frame_table_init ();
	list_push_back (&frame_table, &frame->elem);
}

void
frame_table_remove (struct frame *frame)
{
	if (!list_elem_is_interior (&frame->elem))
		return;
	list_remove (&frame->elem);
}

static uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED) {
	// e에서 struct page *를 복구한 뒤, page->va만 넣어서 hash_bytes 등으로 uint64_t 반환.
	const struct page *page = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&page->va, sizeof(page->va));
}

static bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	const struct page *page_a = hash_entry(a, struct page, hash_elem);
	const struct page *page_b = hash_entry(b, struct page, hash_elem);
	return page_a->va < page_b->va;
}

static void
destroy_page_elem (struct hash_elem *e, void *aux UNUSED)
{
	vm_dealloc_page (hash_entry (e, struct page, hash_elem));
}

/* copy 실패 시 dst에 들어간 page만 해제하고 해시 테이블을 비운다 */
static void
copy_rollback_dst (struct supplemental_page_table *dst)
{
	hash_clear (&dst->hash, destroy_page_elem);
}

static bool copy_parent_anon_fork (struct supplemental_page_table *dst, struct page *p);
static bool copy_parent_file_fork (struct supplemental_page_table *dst, struct page *p);

static bool
copy_parent_as_uninit (struct supplemental_page_table *dst,
		       struct page *parent)
{
	struct page *np;
	bool (*initializer)(struct page *, enum vm_type, void *);
	np = malloc (sizeof *np);
	if (np == NULL)
		return false;
	initializer = parent->uninit.page_initializer;
	if (initializer == NULL)
	  {
		  free (np);
		  return false;
	  }
	uninit_new (np, parent->va, parent->uninit.init, parent->uninit.type,
		    parent->uninit.aux, initializer);
	np->writable = parent->writable;
	if (!spt_insert_page (dst, np))
	  {
		  vm_dealloc_page (np);
		  return false;
	  }
	return true;
}

static bool
copy_parent_anon_fork (struct supplemental_page_table *dst, struct page *parent)
{
	struct page *np = malloc (sizeof *np);
	if (np == NULL)
		return false;
	if (!anon_page_duplicate_for_fork (np, parent))
	  {
		  free (np);
		  return false;
	  }
	if (!spt_insert_page (dst, np))
	  {
		  vm_dealloc_page (np);
		  return false;
	  }
	return true;
}

static bool
copy_parent_file_fork (struct supplemental_page_table *dst, struct page *parent)
{
	struct page *np = malloc (sizeof *np);
	if (np == NULL)
		return false;
	if (!file_page_duplicate_for_fork (np, parent))
	  {
		  free (np);
		  return false;
	  }
	if (!spt_insert_page (dst, np))
	  {
		  vm_dealloc_page (np);
		  return false;
	  }
	return true;
}

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

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
// SPT 키로 쓰이는 VA를 받아서 페이지 생성
 bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	// type이 VM_UNINIT인 경우 예외 처리
	ASSERT (VM_TYPE(type) != VM_UNINIT);

	// 유저 가상 주소인지 확인
	ASSERT (is_user_vaddr(upage));

	// SPT 초기화
	struct supplemental_page_table *spt = &thread_current ()->spt;
	
	// upage를 페이지 크기로 내림
	upage = pg_round_down(upage);
	
	// alignment가 제대로 되었는지 확인
	ASSERT (pg_ofs(upage) == 0);

	struct page *page = NULL;

	// upage가 SPT에 이미 존재하는지 확인
	if (spt_find_page (spt, upage) == NULL) {
		// TODO: Create the page, fetch the initialier according to the VM type,
		page = malloc(sizeof(struct page));

		// 페이지 할당 실패 시 예외 처리
		if (page == NULL) {
			return false;
		}

		// TODO: and then create "uninit" page struct by calling uninit_new. You
		// uninit page 생성
		bool (*initializer)(struct page *, enum vm_type, void *kva) = NULL;
		if (VM_TYPE(type) == VM_ANON) {
			initializer = anon_initializer;
		} else if (VM_TYPE(type) == VM_FILE) {
			initializer = file_backed_initializer;
		}
		if (initializer == NULL) {
			free(page);
			return false;
		}
		uninit_new (page, upage, init, type, aux, initializer);
		
		// TODO: should modify the field after calling the uninit_new. */
		page->writable = writable;

		// TODO: Insert the page into the spt. */
		if (!spt_insert_page (spt, page)) {
			// uninit_new를 지나온 page는 단순한 빈 메모리가 아니므로 vm_dealloc_page로 해제
			vm_dealloc_page(page);
			return false;
		}
		return true;
	}
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page key = { .va = pg_round_down(va) };
	struct hash_elem *e = hash_find(&spt->hash, &key.hash_elem);
	if (e == NULL) {
		return NULL;
	}
	return hash_entry(e, struct page, hash_elem);
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	/* TODO: Fill this function. */
	if(page->va != pg_round_down(page->va)) {
		return false;
	}

	if (hash_insert(&spt->hash, &page->hash_elem) == NULL) {
		return true;
	}

	return false;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	if (hash_delete(&spt->hash, &page->hash_elem) == NULL) {
		return;
	}
	vm_dealloc_page (page);
	return;
}

// clock hand 초기화 helper
static struct list_elem *clock_hand;
static void
clock_hand_init_if_needed (void)
{
	if (clock_hand == NULL || clock_hand == list_end (&frame_table))
		clock_hand = list_begin (&frame_table);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */
	// frame table이 비어있으면 NULL 반환
	if (list_empty (&frame_table))
		return NULL;

	// clock hand 초기화
	clock_hand_init_if_needed ();

	/* 최악의 경우 한 바퀴 이상 돌며 second chance */
	while (true) {
		if (clock_hand == list_end (&frame_table))
			clock_hand = list_begin (&frame_table);
		struct frame *f = list_entry (clock_hand, struct frame, elem);
		clock_hand = list_next (clock_hand); /* 다음 후보를 미리 저장 */
		if (f->page == NULL)
			return f; /* 비어있는 프레임이면 바로 재사용 */
		if (f->pin_count > 0)
			continue;
		if (f->claiming)
			continue;
		struct page *p = f->page;
		struct thread *owner = f->owner;
		if (owner == NULL)
			continue;
		/* accessed=1 이면 bit 내리고 2nd chance (매핑은 owner의 pml4에 있음) */
		if (pml4_is_accessed (owner->pml4, p->va)) {
			pml4_set_accessed (owner->pml4, p->va, false);
			continue;
		}
		/* accessed=0 이면 victim */
		return f;
	}
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	if (victim == NULL)
		return NULL;

	/* 연결된 page가 있으면 backing store로 내보냄 */
	if (victim->page != NULL) {
		struct page *page = victim->page;
		struct thread *owner = victim->owner;
		if (owner == NULL)
			return NULL;
		if (!swap_out (page))
			return NULL;
		/* 기존 매핑 해제 + 양방향 링크 해제 */
		pml4_clear_page (owner->pml4, page->va);
		page->frame = NULL;
		victim->page = NULL;
		victim->owner = NULL;
	}

	/* victim frame 자체(kva)는 재사용 */
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	frame = malloc(sizeof(struct frame));
	if (frame == NULL) {
		return vm_evict_frame();
	}
	//할당 후 0으로 초기화
	memset (frame, 0, sizeof *frame);
	// palloc_get_page(PAL_USER)로 user frame kva를 요청한다.
	void *kva = palloc_get_page(PAL_USER);
	// 실패하면 vm_evict_frame()로 재사용 가능한 frame을 확보한다.
	
	if (kva == NULL) {
		free(frame);
		return vm_evict_frame();
	}

	// 성공한 kva를 struct frame에 저장하고 frame table에 등록한다.
	frame->kva = kva;
	frame->page = NULL;
	frame_table_add(frame);

	ASSERT (frame != NULL);
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
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (addr == NULL)
		return false;
	if (user && !is_user_vaddr (addr))
		return false;

	if (!not_present)
		return write ? vm_handle_wp (spt_find_page (&thread_current()->spt, addr)) : false;

	return vm_claim_page (addr);
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
	struct thread *curr = thread_current();
	/* TODO: Fill this function */
	// 입력 va 정규화 정책을 spt_find_page와 맞춘다.
	void *upage = pg_round_down (va);
	ASSERT (pg_ofs(upage) == 0);

	page = spt_find_page(&curr->spt, upage);
	if (page == NULL) {
		return false;
	}

	// 이미 mapping된 page라면 성공 처리
	if (pml4_get_page(curr->pml4, upage) != NULL) {
		return true;
	}
	
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	// vm_get_frame()으로 frame을 받은 뒤 page->frame과 frame->page를 연결한다.
	if (page == NULL) {
		return false;
	}
	
	struct frame *frame = vm_get_frame ();
	if (frame == NULL) {
		return false;
	}

	/* Set links */
	frame->page = page;
	page->frame = frame;
	frame->owner = thread_current ();
	frame->claiming = true;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// swap_in(page, frame->kva)로 page 내용을 frame에 채운다.
	if (!swap_in(page, frame->kva)) {
		palloc_free_page(frame->kva);
		page->frame = NULL;
		frame->page = NULL;
		frame->owner = NULL;
		frame->claiming = false;
		frame_table_remove(frame);
		free(frame);
		return false;
	}

	// pml4_set_page()로 page->va와 frame->kva를 writable 정책에 맞게 매핑한다.
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)) {
		palloc_free_page(frame->kva);
		page->frame = NULL;
		frame->page = NULL;
		frame->owner = NULL;
		frame->claiming = false;
		frame_table_remove(frame);
		free(frame);
		return false;
	}

	frame->claiming = false;
	return true;
}


bool
vm_pin_user_buffer (const void *uaddr, size_t size) {
	if (size == 0)
		return true;

	struct thread *t = thread_current ();
	uintptr_t start = (uintptr_t) pg_round_down (uaddr);
	uintptr_t last_byte = (uintptr_t) uaddr + (size - 1);
	uintptr_t end_page = (uintptr_t) pg_round_down ((const void *) last_byte);
	uintptr_t p;

	for (p = start; p <= end_page; p += PGSIZE)
		{
			struct page *pg = spt_find_page (&t->spt, (void *) p);
			if (pg == NULL)
				goto rollback;
			if (pg->frame == NULL && !vm_claim_page ((void *) p))
				goto rollback;
			pg = spt_find_page (&t->spt, (void *) p);
			if (pg == NULL || pg->frame == NULL)
				goto rollback;
			pg->frame->pin_count++;
		}
	return true;

rollback:
	for (uintptr_t q = start; q < p; q += PGSIZE)
		{
			struct page *pg = spt_find_page (&t->spt, (void *) q);
			if (pg != NULL && pg->frame != NULL && pg->frame->pin_count > 0)
				pg->frame->pin_count--;
		}
	return false;
}

void
vm_unpin_user_buffer (const void *uaddr, size_t size)
{
	if (size == 0)
		return;

	struct thread *t = thread_current ();
	uintptr_t start = (uintptr_t) pg_round_down (uaddr);
	uintptr_t last_byte = (uintptr_t) uaddr + (size - 1);
	uintptr_t end_page = (uintptr_t) pg_round_down ((const void *) last_byte);

	for (uintptr_t p = start; p <= end_page; p += PGSIZE)
		{
			struct page *pg = spt_find_page (&t->spt, (void *) p);
			if (pg != NULL && pg->frame != NULL && pg->frame->pin_count > 0)
				pg->frame->pin_count--;
		}
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	if(hash_init(&spt->hash, page_hash, page_less, NULL) == false) {
		PANIC("supplemental_page_table_init: hash_init failed");
	}
}

static bool
copy_one_spt_page (struct supplemental_page_table *dst,
		   struct page *parent_page)
{
	switch (VM_TYPE (parent_page->operations->type))
	  {
		  case VM_UNINIT:
			  return copy_parent_as_uninit (dst, parent_page);
		  case VM_ANON:
			  return copy_parent_anon_fork (dst, parent_page);
		  case VM_FILE:
			  return copy_parent_file_fork (dst, parent_page);
		  default:
			  return false;
	  }
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	struct hash_iterator i;
	// caller가 dst에 supplemental_page_table_init을 이미 호출했으니 스킵
	hash_first (&i, &src->hash);
	while (hash_next (&i))
		{
			struct page *parent_page =
				hash_entry (hash_cur (&i), struct page, hash_elem);
			if (!copy_one_spt_page (dst, parent_page))
			{
				copy_rollback_dst (dst);
				return false;
			}
		}
	return true;
}

void supplemental_page_table_kill (struct supplemental_page_table *spt) {
	hash_clear (&spt->hash, destroy_page_elem);
}