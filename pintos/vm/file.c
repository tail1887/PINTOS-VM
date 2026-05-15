/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#include "filesys/file.c"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	struct thread *t = thread_current();
	
	// dirty bits 확인 + frame 여부 확인
	if (pml4_is_dirty(t->pml4, page->va) && page->frame) {
		
		// file_page 정보로 write back
		void *buffer = page->frame->kva;
		off_t size = page->file.read_bytes;
		off_t start = page->file.offset;
		
		if (file_write_at(file_page->file, buffer, size, start) < size) { // 아직 file growth 없음
			return;
		}
	}

	// 매핑 끊기
	pml4_clear_page(t->pml4, page->va);
	
	if (page->frame) {
	// frame 메모리 반환
	palloc_free_page(page->frame->kva);
	// frame 구조체 반환
	free(page->frame);
	}
	
	// file 닫기
	return file_close(file_page->file);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
}

/* Do the munmap */
void
do_munmap (void *addr) {
	void *va = pg_round_down(addr);
	struct thread *curr = thread_current();
	struct supplement_page_table *spt = &curr->spt;
	struct page *upage = spt_find_page(spt, va);
	
	if (!is_user_vaddr(addr) || upage == NULL) {
		return;
	}
	for (int i = 0; i < upage->file.page_cnt; i++) {
		spt_remove_page(spt, upage);
		va = va + PGSIZE;
		upage = spt_find_page(spt, va);
	}
}
