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

	//bitmap_create??鍮꾪듃(slot)瑜?紐뉕컻 留뚮뱾吏 ?レ옄媛 ?꾩슂?쒕뜲
	// disk_size???뱁꽣媛?닔瑜?以섏꽌 PGSIZE / DISK_SECTOR_SIZE(8)濡??섎닠以?
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

// swap ?щ’????λ맂 ?댁슜??kva濡?蹂듦뎄?덉쑝硫?true.
// ?щ’???놁쑝硫?swap 蹂듦뎄???꾩슂 ?놁쓬 ??true. (泥?濡쒕뱶쨌?뚯씪 濡쒕뱶??uninit 寃쎈줈)
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	// ???섏씠吏?????swap ?붿뒪?ъ뿉??媛?몄삱 ?댁슜???놁쓬
	if(anon_page->swap_slot == ANON_SWAP_SLOT_NONE) {
		// swap?먯꽌 蹂듦뎄??
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

	// ?꾨젅?꾩씠 ?녿뒗 寃쎌슦
	if(page->frame == NULL) {
		return false;
	}
	// ?대? ?ㅼ솑 ?щ’???덈뒗 寃쎌슦
	if(anon_page->swap_slot != ANON_SWAP_SLOT_NONE) {
		return false;
	}

	lock_acquire (&swap_lock);

	// 鍮??щ’??李얠븘???좊떦
	size_t swap_slot = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);

	if (swap_slot == BITMAP_ERROR) {
		lock_release(&swap_lock);
		return false;
	}

	// ?붿뒪?ъ뿉 ?댁슜???
	// ?ш린???⑥닔???뱁꽣?⑥쐞濡??곌린 ?뚮Ц??PGSIZE / DISK_SECTOR_SIZE濡??섎닠以?
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

	// ?ㅼ솑 ?щ’???덈뒗 寃쎌슦
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
	// ?좎? VA ??臾쇰━ ?꾨젅??kva) 留ㅽ븨 ?댁젣
	if (t->pml4 != NULL && pml4_get_page (t->pml4, page->va) != NULL)
		pml4_clear_page (t->pml4, page->va);

	vm_frame_table_remove(f);
	// 而ㅻ꼸??硫붾え由?kva) ?댁젣
	if(f->kva != NULL) {
		palloc_free_page(f->kva);
		f->kva = NULL;
	}
	f->page = NULL;
	page->frame = NULL;
	free(f);
}
