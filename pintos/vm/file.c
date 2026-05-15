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
	struct file * mmap_file = file_reopen(file);
	off_t file_size = file_length(file);
	size_t remaining = length;
	void * check_addr = addr;
	while (remaining > 0) {
		if (spt_find_page(&thread_current()->spt, check_addr) != NULL){
			return NULL;
		}
		
	}
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
