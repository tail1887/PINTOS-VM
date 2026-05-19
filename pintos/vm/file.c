/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "string.h"
#include "round.h"
#include "threads/thread.h"

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
	vm_initializer *init = page->uninit.init;
	page->operations = &file_ops;

	if (aux == NULL)
		return true;

	/* segment_aux and mmap file_page aux share the same leading fields. */
	struct file_page *a = aux;
	struct file_page *f = &page->file;
	f->file = a->file;
	f->ofs = a->ofs;
	f->read_bytes = a->read_bytes;
	f->zero_bytes = a->zero_bytes;

	if (init == mmap_lazy_load) {
		f->page_cnt = a->page_cnt;
		f->is_mmap_start = a->is_mmap_start;
	} else {
		f->page_cnt = 0;
		f->is_mmap_start = false;
	}
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
	struct file_page *file_page = &page->file;
	struct frame *frame = page->frame;

	if (frame == NULL || frame->kva == NULL)
		return false;

	uint64_t *pml4 = frame->owner != NULL ? frame->owner->pml4 : thread_current ()-> pml4;

	if (pml4_is_dirty (pml4, page->va)){
		off_t written = file_write_at(file_page->file, frame->kva,
						(off_t) file_page->read_bytes, file_page->ofs);

		if(written != (off_t) file_page->read_bytes)
			return false;
		pml4_set_dirty (pml4, page->va, false);
	}

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

	if (page->frame != NULL) {
		palloc_free_page (page->frame->kva);
		free (page->frame);
		page->frame = NULL;
	}

	if (file_page->file != NULL) {
		/* mmap: 공유 file은 시작 페이지에서만 close. exec lazy는 page_cnt==0이라 각자 close. */
		if (file_page->is_mmap_start || file_page->page_cnt == 0)
			file_close (file_page->file);
		file_page->file = NULL;
	}
}
static void
free_file_aux (void *aux) {
	if (aux != NULL)
		free (aux);
}

static bool
mmap_lazy_load (struct page *page, void *aux) {
	struct file_page *f = &page->file;
	void *kva = page->frame->kva;

	off_t read_bytes = file_read_at (f->file, kva, (off_t) f->read_bytes, f->ofs);

	if (read_bytes != (off_t) f->read_bytes) {
		free_file_aux (aux);
		return false;
	}
	memset ((uint8_t *) kva + read_bytes, 0, f->zero_bytes);
	free_file_aux (aux);
	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t ofs) {

	/* ??二쇱냼??user?곸뿭?몄? 寃?? 二쇱냼怨꾩궛 overflow ?뺤씤 */
	uint8_t *start = addr;
	uint8_t *end = start + length - 1;
	if (end < start || !is_user_vaddr (end)) {
		return NULL;
	}
	/* mmap???꾩튂???대? 湲곗〈 ?섏씠吏媛 議댁옱?섎뒗吏 寃??*/
	struct supplemental_page_table *spt = &thread_current ()->spt;
	size_t check = 0;
	while (check < length) {
		void *upage = (uint8_t *) addr + check;
		if (spt_find_page (spt, upage) != NULL) {
			return NULL;
		}
		check += PGSIZE;
	}

	/* do_mmap?꾩슜 *file???덈줈 媛?몄????쒖옉, file->pos???꾩튂媛 ?щ씪吏??섎룄 ?덇린 ?뚮Ц */
	struct file *mmap_file = file_reopen (file);
	if (mmap_file == NULL) {
		return NULL;
	}

	off_t file_size = file_length (mmap_file);
	if (file_size == 0) {
		file_close (mmap_file);
		return NULL;
	}

	/* length瑜?PGSIZE濡??섎늻怨??щ┝怨꾩궛, 紐뉕컻???섏씠吏媛 ?꾩슂?쒖? */
	size_t page_cnt = DIV_ROUND_UP (length, PGSIZE);

	size_t i = 0;
	while (i < length) {
		struct file_page *file_page = malloc(sizeof *file_page);
		/* file_page媛 留뚮뱾?댁?吏 ?딆븯?ㅻ㈃, SPT???ｌ뿀??mmap page瑜??놁븷以?*/
		if (file_page == NULL) {
			for (size_t j = 0; j < i; j += PGSIZE) {
				struct page *page = spt_find_page (spt, (uint8_t *) addr + j);
				if (page != NULL)
					spt_remove_page (spt, page);
			}
			file_close (mmap_file);
			return NULL;
		}
		//?대쾲 page?먯꽌 mmap ?붿껌 湲곗??쇰줈 泥섎━?댁빞 ???⑥? byte ??
		size_t page_left;

		if (length - i < PGSIZE) {
			page_left = length - i;
		} else {
			page_left = PGSIZE;
		}
		//?꾩옱 ?뚯씪 offset遺???뚯씪 ?앷퉴吏 ?ㅼ젣濡??쎌쓣 ???덈뒗 ?⑥? byte ??
		size_t file_left = 0;

		if (ofs + i < file_size) {
			file_left = file_size - (ofs + i);
		}

		size_t read_bytes;

		if (file_left < page_left) {
			read_bytes = file_left;
		} else {
			read_bytes = page_left;
		}

		size_t zero_bytes = PGSIZE - read_bytes;

		file_page->file = mmap_file;
		file_page->ofs = ofs + i;
		file_page->read_bytes = read_bytes;
		file_page->zero_bytes = zero_bytes;
		/* mmap 援ш컙 ?앸퀎: page_cnt??紐⑤뱺 mmap ?섏씠吏???숈씪?섍쾶 ?붾떎. */
		file_page->page_cnt = page_cnt;
		file_page->is_mmap_start = (i == 0);

		if (!vm_alloc_page_with_initializer (VM_FILE, (uint8_t *) addr + i, writable,
				mmap_lazy_load, file_page)) {

			free(file_page);

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
	size_t page_cnt = upage->file.page_cnt;
	for (size_t i = 0; i < page_cnt; i++) {
		if (upage == NULL)
			break;
		spt_remove_page(spt, upage);
		va = (uint8_t *) va + PGSIZE;
		upage = spt_find_page(spt, va);
	}
}
