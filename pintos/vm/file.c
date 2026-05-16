/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/mmu.h"

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
	void *aux = page->uninit.aux;
	page->operations = &file_ops;

	if(aux != NULL) {
		// 원래 uninit에 있던 정보를 가져와서 파일 페이지로 이동시킴
		struct segment_aux *a = aux;
		struct file_page *f = &page->file;
		f->file = a->file;
		f->ofs = a->ofs;
		f->read_bytes = a->read_bytes;
		f->zero_bytes = a->zero_bytes;
		f->page_cnt = 0;
		return true;
	}
	return false;
}

/* Swap in the page by read contents from the file. */
//페이지가 이미 VM_FILE로 바뀐 뒤, 프레임이 없어졌다가(이빅션 등) 다시 swap_in이 불릴 때 호출됨
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *f = &page->file;
	off_t read_bytes = file_read_at (f->file, kva, (off_t) f->read_bytes,
			f->ofs);
	if (read_bytes != f->read_bytes){
		return false;
	}
	memset(kva + read_bytes, 0, f->zero_bytes);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct frame *f = page->frame;
	if (f == NULL) {
		return;
	}

	struct thread *t = thread_current ();
	if (t->pml4 != NULL && pml4_get_page (t->pml4, page->va) != NULL)
		pml4_clear_page (t->pml4, page->va);
	
	if(f->kva != NULL) {
		palloc_free_page(f->kva);
		f->kva = NULL;
	}
	f->page = NULL;
	page->frame = NULL;
	free(f);	
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
