/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "string.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool mmap_lazy_load (struct page *page, void *aux);

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

	if (aux == NULL)
		return false;

	/* segment_aux and mmap file_page aux share the same leading fields;
	   mmap aux also sets page_cnt / is_mmap_start (segment aux page is zeroed). */
	struct file_page *a = aux;
	struct file_page *f = &page->file;
	f->file = a->file;
	f->ofs = a->ofs;
	f->read_bytes = a->read_bytes;
	f->zero_bytes = a->zero_bytes;
	f->page_cnt = a->page_cnt;
	f->is_mmap_start = a->is_mmap_start;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *f = &page->file;
	off_t read_bytes = file_read_at (f->file, kva, (off_t) f->read_bytes,
			f->ofs);
	if (read_bytes != f->read_bytes)
		return false;
	memset (kva + read_bytes, 0, f->zero_bytes);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	return true;
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
		off_t start = page->file.ofs;

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

static void
free_file_aux (void *aux) {
	if (aux != NULL)
		palloc_free_page (aux);
}

static bool
mmap_lazy_load (struct page *page, void *aux) {
	struct file_page *a = aux;

	void *kva = page->frame->kva;

	off_t read_bytes = file_read_at (a->file, kva, (off_t) a->read_bytes, a->ofs);
	if (read_bytes != a->read_bytes) {
		free_file_aux (a);
		return false;
	}
	memset (kva + read_bytes, 0, a->zero_bytes);
	free_file_aux (a);
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t ofs) {

	/* 끝 주소도 user영역인지 검사, 주소계산 overflow 확인 */
	uint8_t *start = addr;
	uint8_t *end = start + length - 1;
	if (end < start || !is_user_vaddr (end)) {
		return NULL;
	}
	/* mmap할 위치에 이미 기존 페이지가 존재하는지 검사 */
	struct supplemental_page_table *spt = &thread_current ()->spt;
	size_t check = 0;
	while (check < length) {
		void *upage = (uint8_t *) addr + check;
		if (spt_find_page (spt, upage) != NULL) {
			return NULL;
		}
		check += PGSIZE;
	}

	/* do_mmap전용 *file을 새로 가져와서 시작, file->pos의 위치가 달라질 수도 있기 때문 */
	struct file *mmap_file = file_reopen (file);
	if (mmap_file == NULL) {
		return NULL;
	}

	off_t file_size = file_length (mmap_file);
	if (file_size == 0) {
		file_close (mmap_file);
		return NULL;
	}

	/* length를 PGSIZE로 나누고 올림계산, 몇개의 페이지가 필요한지 */
	size_t page_cnt = DIV_ROUND_UP (length, PGSIZE);

	size_t i = 0;
	while (i < length) {
		struct file_page *file_page = palloc_get_page (0);
		/* file_page가 만들어지지 않았다면, SPT에 넣었던 mmap page를 없애줌 */
		if (file_page == NULL) {
			for (size_t j = 0; j < i; j += PGSIZE) {
				struct page *page = spt_find_page (spt, (uint8_t *) addr + j);
				if (page != NULL)
					spt_remove_page (spt, page);
			}
			file_close (mmap_file);
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
		/* mmap한 첫번째 페이지에 몇개의 페이지를 mmap했는지 저장 */
		if (i == 0) {
			file_page->is_mmap_start = true;
			file_page->page_cnt = page_cnt;
		} else {
			file_page->is_mmap_start = false;
			file_page->page_cnt = 0;
		}

		if (!vm_alloc_page_with_initializer (VM_FILE, (uint8_t *) addr + i, writable,
				mmap_lazy_load, file_page)) {

			palloc_free_page (file_page);

			for (size_t j = 0; j < i; j += PGSIZE) {
				struct page *page = spt_find_page (spt, (uint8_t *) addr + j);
				if (page != NULL)
					spt_remove_page (spt, page);
			}
			file_close (mmap_file);
			return NULL;
		}

		i += PGSIZE;
	}
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	void *va = pg_round_down(addr);
	struct thread *curr = thread_current();
	struct supplemental_page_table *spt = &curr->spt;
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
