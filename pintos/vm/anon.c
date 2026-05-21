/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "threads/synch.h"
#include "lib/kernel/bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static struct bitmap *swap_bitmap;
static struct lock swap_lock;

static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);

	if(swap_disk == NULL) {
		PANIC("Failed to get swap disk");
	}

	//bitmap_create는 비트(slot)를 몇개 만들지 숫자가 필요한데 
	// disk_size는 섹터갯수를 줘서 PGSIZE / DISK_SECTOR_SIZE(8)로 나눠줌
	size_t swap_slot_cnt = disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE);
	if (swap_slot_cnt == 0) {
		PANIC("Failed to calculate swap slot count");
	}
	
	swap_bitmap = bitmap_create(swap_slot_cnt);
	if(swap_bitmap == NULL) {
		PANIC("Failed to create swap bitmap");
	}

	lock_init(&swap_lock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	anon_page->swap_slot = ANON_SWAP_SLOT_NONE;
	return true;
}

// swap 슬롯에 저장된 내용을 kva로 복구했으면 true. 
// 슬롯이 없으면 swap 복구는 필요 없음 → true. (첫 로드·파일 로드는 uninit 경로)
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	// 이 페이지에 대해 swap 디스크에서 가져올 내용이 없음
	if(anon_page->swap_slot == ANON_SWAP_SLOT_NONE) {
		// swap에서 복구할
		return true;
	}
	
	size_t swap_slot = anon_page->swap_slot;
	disk_sector_t base = swap_slot * (PGSIZE / DISK_SECTOR_SIZE);
    for (size_t i = 0; i < PGSIZE / DISK_SECTOR_SIZE; i++){
        disk_read (swap_disk, base + i, kva + i * DISK_SECTOR_SIZE);
    }

	lock_acquire(&swap_lock);
	bitmap_reset(swap_bitmap, swap_slot);
	anon_page->swap_slot = ANON_SWAP_SLOT_NONE;
	lock_release(&swap_lock);
	
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	// 프레임이 없는 경우
	if(page->frame == NULL) {
		return false;
	}
	// 이미 스왑 슬롯에 있는 경우
	if(anon_page->swap_slot != ANON_SWAP_SLOT_NONE) {
		return false;
	}

	lock_acquire (&swap_lock);

	// 빈 슬롯을 찾아서 할당
	size_t swap_slot = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
	
	if (swap_slot == BITMAP_ERROR) {
		lock_release(&swap_lock);
		return false;
	}

	// 디스크에 내용을 씀
	// 여기서 함수는 섹터단위로 쓰기 때문에 PGSIZE / DISK_SECTOR_SIZE로 나눠줌
	void *kva = page->frame->kva;
	disk_sector_t base = swap_slot * (PGSIZE / DISK_SECTOR_SIZE);
	for (size_t i = 0; i < PGSIZE / DISK_SECTOR_SIZE; i++) { 
		disk_write(swap_disk, base + i, kva + i * DISK_SECTOR_SIZE);
	}

	anon_page->swap_slot = swap_slot;

	lock_release(&swap_lock);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {

	// 스왑 슬롯이 있는 경우
	struct anon_page *anon_page = &page->anon;
	if(anon_page->swap_slot != ANON_SWAP_SLOT_NONE) {
		lock_acquire(&swap_lock);
		bitmap_reset(swap_bitmap, anon_page->swap_slot);
		lock_release(&swap_lock);
		anon_page->swap_slot = ANON_SWAP_SLOT_NONE;
	}

	struct frame *f = page->frame;
	if (f == NULL) {
		return;
	}

	struct thread *t = thread_current ();
	// 유저 VA → 물리 프레임(kva) 매핑 해제
	if (t->pml4 != NULL && pml4_get_page (t->pml4, page->va) != NULL)
		pml4_clear_page (t->pml4, page->va);

	vm_frame_table_remove(f);
	// 커널용 메모리(kva) 해제
	if(f->kva != NULL) {
		palloc_free_page(f->kva);
		f->kva = NULL;
	}
	f->page = NULL;
	page->frame = NULL;
	free(f);
}