/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool mmap_initializer (struct page *page, void *aux);

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

static bool
mmap_initializer (struct page *page, void *aux) {
	struct file_page *file_page = aux;
	
	page->file.file = file_page->file;
	page->file.offset = file_page->offset;
	page->file.read_bytes = file_page->read_bytes;
	page->file.zero_bytes = file_page->zero_bytes;
	page->file.is_mmap_start = file_page->is_mmap_start;
	page->file.page_cnt = file_page->page_cnt;

	palloc_free_page(file_page);
	return true; 
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t ofs) {

	//끝 주소도 user영역인지 검사, 주소계산 overflow 확인
	uint8_t *start = addr;
	uint8_t *end = start + length - 1;
	if (end < start || !is_user_vaddr (end)) {
		return NULL;
	}
	//mmap할 위치에 이미 기존 페이지가 존재하는지 검사
	struct supplemental_page_table *spt = &thread_current ()->spt;
	size_t check = 0;

	while (check < length) {
		void *upage = (uint8_t *) addr + check;
		if (spt_find_page (spt, upage) != NULL) {
			return NULL;
		}
		check += PGSIZE;
	}
	
	//do_mmap전용 file을 복제후 시작
	struct file * mmap_file = file_reopen(file);
	if (mmap_file == NULL){
		return NULL;
	}
	off_t file_size = file_length(mmap_file);
	size_t page_cnt = DIV_ROUND_UP(length, PGSIZE);
	size_t i = 0;
	while(i < length){
		struct file_page *file_page = palloc_get_page(0);

		if (file_page == NULL){
			for(size_t j = 0 ; j < i ; j += PGSIZE){
				struct page *page = spt_find_page(spt, (uint8_t *)addr + j);
				if(page != NULL)
					spt_remove_page(spt, page);
			}
			file_close(mmap_file);
			return NULL;
		}

		size_t page_left = length - i < PGSIZE ? length - i : PGSIZE;
		size_t file_left = 0;
		if (ofs + i < file_size)
			file_left = file_size - (ofs + i);

		size_t read_bytes = file_left < page_left ? file_left : page_left;
		size_t zero_bytes = PGSIZE - read_bytes;

		file_page->file = mmap_file;
		file_page->ofs = ofs + i;
		file_page->read_bytes = read_bytes;
		file_page->zero_bytes = zero_bytes;
		//mmap한 첫번째 페이지에 몇개의 페이지를 mmap했는지 저장
		if (i == 0) {
			file_page->is_mmap_start = true;
			file_page->page_cnt = page_cnt;
		} else {
			file_page->is_mmap_start = false;
			file_page->page_cnt = 0;
		}

		if(!vm_alloc_page_with_initializer(VM_FILE, (uint8_t *)addr + i, writable, 
													mmap_initializer, file_page)){
			
			palloc_free_page(file_page);
			struct supplemental_page_table *spt = &thread_current()->spt;
			for(size_t j = 0 ; j < i ; j += PGSIZE){
				struct page *page = spt_find_page(spt, (uint8_t *)addr + j);
				if(page != NULL)
					spt_remove_page(spt, page);
			}
			file_close(mmap_file);
			return NULL;
		}
		
		i += PGSIZE;
	}
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
