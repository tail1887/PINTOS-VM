/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include <debug.h>
#include <string.h>
#include "vm/vm.h"
#include "vm/anon.h"
#include "vm/uninit.h"
#include "vm/inspect.h"
#include "filesys/file.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"

static struct list frame_table;
static struct lock frame_table_lock;
static struct list_elem *clock_hand;
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
	list_init (&frame_table);
	lock_init (&frame_table_lock);
	clock_hand = NULL;
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
static void spt_page_destructor (struct hash_elem *e, void *aux);
static void *spt_copy_uninit_aux (struct page *src);
static bool spt_copy_uninit_page (struct page *src);
static bool spt_copy_loaded_anon_page (struct page *src);
static void spt_copy_rollback (struct supplemental_page_table *dst);

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

static void
frame_table_add (struct frame *frame){
	lock_acquire (&frame_table_lock);
	list_push_back (&frame_table, &frame->elem);
	frame->in_frame_table = true;
	if (clock_hand == NULL)
		clock_hand = list_begin (&frame_table);
	lock_release (&frame_table_lock);
}

void
vm_frame_table_remove (struct frame *frame) {
	if(frame == NULL || !frame->in_frame_table)
		return;
	lock_acquire(&frame_table_lock);

	if (clock_hand == &frame->elem) {
		clock_hand = list_next (clock_hand);
		if (clock_hand == list_end (&frame_table))
			clock_hand = list_begin (&frame_table);
	}

	list_remove(&frame->elem);
	frame->in_frame_table = false;

	if(list_empty(&frame_table))
		clock_hand = NULL;

	lock_release(&frame_table_lock);
}

static struct frame *
frame_table_next (void) {
	if (list_empty (&frame_table))
		return NULL;

	if(clock_hand == NULL || clock_hand == list_end(&frame_table))
		clock_hand = list_begin(&frame_table);

	struct frame *frame = list_entry(clock_hand, struct frame, elem);

	clock_hand = list_next(clock_hand);
	if(clock_hand == list_end(&frame_table))
		clock_hand= list_begin(&frame_table);

	return frame;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	void *va = NULL;
	uint64_t *pml4 = NULL;

	lock_acquire(&frame_table_lock);
	
	 /* TODO: The policy for eviction is up to you. */
	for(int i = 0; i < (int)list_size(&frame_table); i++){
		victim = frame_table_next();
		if (victim == NULL){
			lock_release(&frame_table_lock);
			return NULL;
		}
		if (victim->page == NULL || victim->owner == NULL)
			continue;

		va = victim->page->va;
		pml4 = victim->owner->pml4;
		if (va == NULL || pml4 == NULL)
			continue;

		if(pml4_is_accessed(pml4, va)){
			pml4_set_accessed(pml4, va, false);
			continue;
		}
		lock_release(&frame_table_lock);
		return victim;
	}

	//두번째 순회는 기회 안주고 즉시 반환
	for(int i = 0; i < (int)list_size(&frame_table); i++){
		victim = frame_table_next();
		if (victim == NULL){
			lock_release(&frame_table_lock);
			return NULL;
		}
		if (victim->page == NULL || victim->owner == NULL)
			continue;

		va = victim->page->va;
		pml4 = victim->owner->pml4;

		if (va == NULL || pml4 == NULL)
			continue;

		lock_release(&frame_table_lock);
		return victim;
	}
	lock_release(&frame_table_lock);
	return NULL;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	//victim할 frame 가져오기
	struct frame *victim  = vm_get_victim ();
	if (victim == NULL) {
		return NULL;
	}
	struct page *page = victim->page;
	if (page == NULL) {
		return NULL;
	}
	//victim의 page를 swap_out
	if (!swap_out(page)) {
		return NULL;
	}
	/* Victim may belong to another process. */
	struct thread *owner = victim->owner != NULL ? victim->owner : thread_current ();
	pml4_clear_page (owner->pml4, page->va);
	ASSERT (pml4_get_page (owner->pml4, page->va) == NULL);

	victim->page = NULL;
	page->frame = NULL;
	
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {

	struct frame *frame = NULL;
	frame = malloc (sizeof *frame);
	if (frame == NULL)
		return NULL;

	/* TODO: Fill this function. */
	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva == NULL) {
		free (frame);
		frame = vm_evict_frame ();
		if (frame == NULL)
			return NULL;
	} else {
		frame->page = NULL;
		frame->owner = NULL;
		frame->in_frame_table = false;
		frame_table_add (frame);
	}

	if (frame == NULL)
		return NULL;
	frame->page = NULL;
	return frame;
}

/* Growing the stack. */
bool
vm_stack_growth (void *addr) {
	addr = pg_round_down(addr);

	if (!vm_alloc_page_with_initializer(VM_ANON, addr, true, NULL, NULL))
		return false;

	if (vm_claim_page(addr))
		return true;

	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page *page = spt_find_page(spt, addr);
	if (page != NULL)
		spt_remove_page(spt, page);
	return false;
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
	return false;
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	//page_table에 없는지 검사
	if (!not_present){
		return false;
	}
	//유저모드 주소인지 검사
	if (addr == NULL || !is_user_vaddr(addr)){
		return false;
	}
	//spt에 page가 있다면 바로 claim
	page = spt_find_page(spt, addr);
	if (page != NULL) {
		//쓰기 권한 위반이 아닌지 검사
		if (write && !page->writable) {
			return false;
		}
		return vm_do_claim_page(page);
	}
	//spt에 page가 없다면, stack_growth검사
	if (vm_can_stack_growth(f, addr, user)){
		return vm_stack_growth(addr);
	}
	return false;
}

//스택그로스 검사 헬퍼함수
bool
vm_can_stack_growth (struct intr_frame *f, void *addr, bool user){
	//user모드 fault인지, kernel모드 fault인지 분리해서 검사
	uintptr_t va = (uintptr_t) addr;
	uintptr_t stack_bottom_limit = (uintptr_t)USER_STACK - (1 << 20);
	uintptr_t stack_top = (uintptr_t)USER_STACK;
	struct thread *curr = thread_current();

	//fault_addr가 스택 범위 내에 있는지
	if (va < stack_bottom_limit || va >= stack_top){
		return false;
	}
	//user모드에서 page_fault인 경우
	if (user){
		if (f->rsp < 8){
			return false;
		}
		if (va < f->rsp - 8){
			return false;
		}
	} else {
	//kernel모드에서 page_fault인 경우
		if (curr->user_rsp < 8){
			return false;
		}
		if (va < curr->user_rsp - 8){
				return false;
		}
	}
	return true;
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
	if (page == NULL)
		return false;
	return vm_do_claim_page (page);
}

/* Roll back a failed claim. Reused (evicted) frames stay in the table. */
static void
vm_undo_failed_claim (struct frame *frame, struct page *page,
		struct thread *prev_owner) {
	page->frame = NULL;
	frame->page = NULL;
	frame->owner = prev_owner;
	if (prev_owner != NULL)
		return;
	vm_frame_table_remove (frame);
	palloc_free_page (frame->kva);
	free (frame);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (frame == NULL)
		return false;
	/* Evicted frames keep prev_owner; new frames have owner == NULL. */
	struct thread *prev_owner = frame->owner;
	frame->page = page;
	frame->owner = thread_current ();
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	void *upage = page->va;
	void *kpage = pg_round_down (frame->kva);

	if (!pml4_set_page (thread_current ()->pml4, upage, kpage,
			page->writable)) {
		vm_undo_failed_claim (frame, page, prev_owner);
		return false;
	}

	if (!swap_in (page, frame->kva)) {
		pml4_clear_page (thread_current ()->pml4, upage);
		vm_undo_failed_claim (frame, page, prev_owner);
		return false;
	}
	return true;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	if (!hash_init (&spt->hash, page_hash, page_less, NULL))
		PANIC ("supplemental_page_table_init: hash_init failed");
}

/* spt kill·copy 실패 롤백용: hash entry마다 page 전체 해제 */
static void
spt_page_destructor (struct hash_elem *e, void *aux UNUSED) {
	struct page *p = hash_entry (e, struct page, elem);
	vm_dealloc_page (p);
}

/* Merge 5-A: 부모 uninit page의 aux 페이지를 깊은 복사한다.
 * exec segment_aux·mmap file_page aux는 선두 필드가 struct file*이므로 file_reopen. */
static void *
spt_copy_uninit_aux (struct page *src) {
	struct uninit_page *u = &src->uninit;

	if (u->aux == NULL)
		return NULL;

	void *child_aux = palloc_get_page (PAL_ZERO);
	if (child_aux == NULL)
		return NULL;

	memcpy (child_aux, u->aux, PGSIZE);

	if (VM_TYPE (page_get_type (src)) == VM_FILE) {
		struct file_page *a = child_aux;
		if (a->file != NULL) {
			struct file *reopened = file_reopen (a->file);
			if (reopened == NULL) {
				palloc_free_page (child_aux);
				return NULL;
			}
			a->file = reopened;
		}
	}
	return child_aux;
}

/* Merge 5-A: 부모 UNINIT 한 엔트리를 자식 SPT에 lazy 상태로 등록한다.
 * frame·swap 슬롯은 복사하지 않는다. (__do_fork 시점에 current는 자식) */
static bool
spt_copy_uninit_page (struct page *src) {
	struct uninit_page *u = &src->uninit;
	void *child_aux = spt_copy_uninit_aux (src);

	if (u->aux != NULL && child_aux == NULL)
		return false;

	if (!vm_alloc_page_with_initializer (page_get_type (src), src->va,
			src->writable, u->init, child_aux)) {
		if (child_aux != NULL)
			palloc_free_page (child_aux);
		return false;
	}
	return true;
}

/* copy 실패 시 자식 SPT에 이미 넣은 page 정리 */
static void
spt_copy_rollback (struct supplemental_page_table *dst) {
	hash_destroy (&dst->hash, spt_page_destructor);
	hash_init (&dst->hash, page_hash, page_less, NULL);
}

/* Merge 5-B: 부모 loaded anon을 자식에 등록·claim 후 내용을 복사한다 (즉시 복사, COW 아님). */
static bool
spt_copy_loaded_anon_page (struct page *src) {
	void *va = src->va;
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *child = NULL;
	void *child_kva = NULL;

	if (!vm_alloc_page (VM_ANON, va, src->writable))
		return false;

	if (!vm_claim_page (va)) {
		child = spt_find_page (spt, va);
		if (child != NULL)
			spt_remove_page (spt, child);
		return false;
	}

	child = spt_find_page (spt, va);
	if (child == NULL || child->frame == NULL)
		return false;

	child_kva = child->frame->kva;

	if (src->frame != NULL) {
		memcpy (child_kva, src->frame->kva, PGSIZE);
	} else if (src->anon.swap_slot != ANON_SWAP_SLOT_NONE) {
		/* 부모만 swap out된 경우: 슬롯은 건드리지 않고 디스크 내용만 읽는다 */
		if (!anon_read_swap_to_kva (src->anon.swap_slot, child_kva)) {
			spt_remove_page (spt, child);
			return false;
		}
	} else {
		spt_remove_page (spt, child);
		return false;
	}
	return true;
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	struct hash_elem *e;
	enum vm_type ty;

	ASSERT (dst != NULL && src != NULL);

	for (e = hash_begin (&src->hash); e != hash_end (&src->hash);
			e = hash_next (e)) {
		struct page *src_page = hash_entry (e, struct page, elem);
		ty = VM_TYPE (src_page->operations->type);

		if (ty == VM_UNINIT) {
			if (!spt_copy_uninit_page (src_page)) {
				spt_copy_rollback (dst);
				return false;
			}
		} else if (ty == VM_ANON) {
			if (!spt_copy_loaded_anon_page (src_page)) {
				spt_copy_rollback (dst);
				return false;
			}
		} else {
			/* Merge 5-C: file-backed / mmap */
			spt_copy_rollback (dst);
			return false;
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy (&(spt->hash), spt_page_destructor);
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
