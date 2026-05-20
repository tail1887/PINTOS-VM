# Merge 4 Swap 학습자료

## 시작 전 이해 확인 질문

이 문서를 공부한 뒤에는 아래 질문에 답할 수 있어야 한다.

```text
1. Merge 4에서 eviction이 필요한 이유는 무엇인가?
2. eviction은 page를 삭제하는 작업인가, frame을 재사용하는 작업인가?
3. frame table은 왜 필요하고, 어떤 frame들이 들어가야 하는가?
4. victim frame은 어떤 기준으로 고를 수 있는가?
5. accessed bit와 dirty bit는 각각 어디에 쓰이는가?
6. anon page가 eviction될 때 왜 swap disk가 필요한가?
7. swap slot 하나는 왜 page 하나와 같은 크기로 생각하는가?
8. anon_swap_out()은 정확히 무엇을 저장하고, 어디에 저장하는가?
9. anon_swap_in()은 언제 호출되고, 무엇을 복구하는가?
10. file-backed page는 왜 dirty일 때만 파일에 write-back 하면 되는가?
11. vm_evict_frame()에서 swap_out 후 pml4_clear_page()를 해야 하는 이유는 무엇인가?
12. eviction 후 page는 SPT에 남아 있어야 하는가?
13. vm_get_frame()에서 palloc_get_page()가 실패하면 어떤 흐름으로 넘어가야 하는가?
14. file_backed_swap_out()과 file_backed_destroy()는 무엇이 다른가?
15. swap_in과 swap_out은 왜 page 타입별 함수 포인터로 나뉘어 있는가?
```

맨 마지막에 같은 질문에 대한 답을 정리해두었다.

## 0. 한눈에 보는 목차

1. Merge 4가 해결하려는 문제
2. frame, page, swap의 관계
3. 전체 흐름 한 장 요약
4. Frame Table
5. Victim Selection
6. Eviction Flow
7. Anonymous Swap
8. File-backed Swap Out
9. Swap In과 Swap Out의 차이
10. 함수별 역할 정리
11. 구현 순서
12. 자주 헷갈리는 지점
13. 디버깅 체크포인트
14. 테스트가 보는 것

---

# 1. Merge 4가 해결하려는 문제

Merge 1~3까지는 page fault가 났을 때 새 frame을 가져와서 page와 연결했다.

```text
page fault
-> SPT에서 page 찾기
-> frame 확보
-> page table에 va -> frame 매핑
-> swap_in으로 frame 내용 채우기
```

그런데 물리 메모리는 무한하지 않다.

`palloc_get_page(PAL_USER)`가 항상 성공할 수 없다.

유저 풀이 꽉 차면 새 frame을 줄 수 없다.

이때 운영체제가 해야 할 일은 다음과 같다.

```text
1. 지금 메모리에 올라와 있는 frame 중 하나를 고른다.
2. 그 frame에 들어 있던 page 내용을 안전한 곳에 백업한다.
3. page table 매핑을 끊는다.
4. page와 frame의 연결을 끊는다.
5. 비워진 frame을 새 page에게 재사용한다.
```

이 흐름이 eviction이다.

Merge 4의 핵심은 한 문장으로 정리할 수 있다.

```text
메모리가 부족할 때 기존 page를 안전하게 내보내고, 그 frame을 새 page에게 재사용한다.
```

---

# 2. frame, page, swap의 관계

## 2.1 page

`struct page`는 유저 가상 주소 하나를 설명하는 커널 자료구조다.

중요한 필드는 다음과 같다.

```c
struct page {
	const struct page_operations *operations;
	void *va;
	struct frame *frame;
	struct hash_elem elem;
	bool writable;

	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
	};
};
```

page는 다음 정보를 기억한다.

```text
이 page의 유저 가상 주소는 무엇인가?
지금 frame과 연결되어 있는가?
쓰기 가능한 page인가?
anon page인가 file-backed page인가?
나중에 swap_in/swap_out/destroy를 어떤 함수로 처리해야 하는가?
```

## 2.2 frame

개념적으로 frame은 물리 메모리의 한 page 크기 공간이다.

하지만 코드에서 `struct frame`은 그 물리 공간을 설명하는 커널 자료구조다.

```c
struct frame {
	void *kva;
	struct page *page;
	struct list_elem elem;
};
```

각 필드의 의미는 다음과 같다.

```text
kva
  실제 물리 frame에 커널이 접근할 때 쓰는 커널 가상 주소

page
  이 frame을 현재 사용 중인 page

elem
  frame_table 리스트에 들어가기 위한 연결고리
```

## 2.3 swap

swap은 메모리에서 쫓겨난 page 내용을 잠시 저장하는 공간이다.

anonymous page는 원본 파일이 없다.

그래서 eviction될 때 내용이 사라지면 복구할 방법이 없다.

따라서 anon page는 swap disk에 저장해야 한다.

file-backed page는 원본 파일이 있다.

수정되지 않은 file-backed page는 그냥 버려도 된다.

나중에 다시 필요하면 원본 파일에서 다시 읽으면 된다.

하지만 수정된 file-backed page는 파일에 write-back 해야 한다.

---

# 3. 전체 흐름 한 장 요약

## 3.1 frame이 충분할 때

```text
page fault
-> vm_try_handle_fault()
-> spt_find_page()
-> vm_do_claim_page()
-> vm_get_frame()
-> palloc_get_page(PAL_USER) 성공
-> page와 frame 연결
-> pml4_set_page()
-> swap_in()
-> 유저 프로그램 계속 실행
```

## 3.2 frame이 부족할 때

```text
page fault
-> vm_try_handle_fault()
-> spt_find_page()
-> vm_do_claim_page()
-> vm_get_frame()
-> palloc_get_page(PAL_USER) 실패
-> vm_evict_frame()
-> vm_get_victim()
-> victim page swap_out()
-> pml4_clear_page()
-> victim page와 frame 연결 해제
-> 비워진 frame 반환
-> 새 page와 frame 연결
-> pml4_set_page()
-> swap_in()
-> 유저 프로그램 계속 실행
```

## 3.3 그림으로 보기

```text
새 page가 frame 필요
        |
        v
vm_get_frame()
        |
        +-- free frame 있음
        |       |
        |       v
        |   palloc_get_page()
        |       |
        |       v
        |   새 frame 생성
        |
        +-- free frame 없음
                |
                v
            vm_evict_frame()
                |
                v
            vm_get_victim()
                |
                v
            swap_out(victim->page)
                |
                v
            pml4_clear_page()
                |
                v
            victim frame 재사용
```

---

# 4. Frame Table

## 4.1 Frame Table이 필요한 이유

eviction을 하려면 현재 메모리에 올라와 있는 frame들을 볼 수 있어야 한다.

어떤 frame을 내보낼지 고르려면 frame 목록이 필요하다.

그 목록이 frame table이다.

```text
frame_table
-> 현재 유저 풀에서 할당되어 사용 중인 frame들의 리스트
```

## 4.2 어디에 선언하는가

보통 `vm/vm.c`에 전역 static 리스트로 둔다.

```c
static struct list frame_table;
```

`static`을 붙이면 이 리스트는 `vm.c` 안에서만 접근 가능하다.

frame table은 VM 내부 구현 세부사항이므로 외부에 공개하지 않아도 된다.

## 4.3 어디에서 초기화하는가

VM 시스템이 시작될 때 한 번 초기화한다.

```c
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS
	pagecache_init ();
#endif
	register_inspect_intr ();
	list_init (&frame_table);
}
```

`vm_init()`은 VM subsystem의 시작점이다.

따라서 frame table도 여기서 초기화하는 것이 자연스럽다.

## 4.4 언제 frame_table에 넣는가

새 frame을 처음 만들 때 넣는다.

```text
palloc_get_page(PAL_USER) 성공
-> struct frame malloc
-> frame->kva 설정
-> frame->page = NULL
-> list_push_back(&frame_table, &frame->elem)
```

주의할 점이 있다.

eviction으로 재사용한 frame은 이미 frame_table 안에 있다.

따라서 eviction으로 받은 frame을 다시 `list_push_back()` 하면 안 된다.

```text
새 frame
  frame_table에 추가

evicted frame 재사용
  이미 frame_table에 있으므로 다시 추가하지 않음
```

---

# 5. Victim Selection

## 5.1 victim이란?

victim은 메모리에서 내보낼 frame이다.

즉, eviction의 대상이다.

```text
victim frame
-> 현재 어떤 page를 담고 있음
-> 이 page를 swap_out 하고
-> frame을 비운 뒤
-> 새 page에게 재사용
```

## 5.2 아무 frame이나 고르면 안 되는 이유

아무 frame이나 고르면 최근에 자주 쓰는 page를 내보낼 수 있다.

그러면 바로 다시 page fault가 발생하고, 성능이 나빠진다.

그래서 최근에 안 쓰인 page를 고르는 정책이 필요하다.

Pintos에서는 보통 accessed bit를 이용한 second chance 방식을 쓴다.

## 5.3 accessed bit

CPU는 어떤 page가 접근되면 page table entry의 accessed bit를 켠다.

커널은 이 bit를 보고 최근 사용 여부를 추정할 수 있다.

```text
accessed bit == true
  최근에 접근된 page일 가능성이 높음

accessed bit == false
  최근에 접근되지 않은 page일 가능성이 높음
```

관련 함수는 보통 다음과 같다.

```c
pml4_is_accessed (pml4, va);
pml4_set_accessed (pml4, va, false);
```

## 5.4 Second Chance 방식

second chance는 이름 그대로 한 번 더 기회를 주는 방식이다.

```text
frame_table을 순회한다.

accessed bit가 true이면:
  최근 사용된 page이므로 바로 내보내지 않는다.
  accessed bit를 false로 바꾼다.
  다음 frame을 본다.

accessed bit가 false이면:
  최근 사용되지 않은 page로 보고 victim으로 고른다.
```

의사코드는 다음과 같다.

```c
static struct frame *
vm_get_victim (void) {
	while (true) {
		struct frame *frame = next_frame_from_frame_table ();
		struct page *page = frame->page;

		if (page == NULL)
			return frame;

		if (pml4_is_accessed (thread_current ()->pml4, page->va)) {
			pml4_set_accessed (thread_current ()->pml4, page->va, false);
			continue;
		}

		return frame;
	}
}
```

실제 구현에서는 `thread_current()->pml4`만 쓰면 fork나 여러 프로세스 상황에서 부족할 수 있다.

page가 속한 process의 pml4를 어떻게 찾을지 팀 설계가 필요할 수 있다.

간단한 단계에서는 현재 실행 중인 프로세스의 frame을 대상으로 통과할 수 있지만, 완성도를 높이려면 owner 정보를 frame/page에 두는 방식도 고려한다.

---

# 6. Eviction Flow

## 6.1 vm_evict_frame의 역할

`vm_evict_frame()`은 victim frame을 실제로 비우는 함수다.

`vm_get_victim()`이 고르는 함수라면, `vm_evict_frame()`은 정리하는 함수다.

```text
vm_get_victim()
  어떤 frame을 비울지 고름

vm_evict_frame()
  그 frame 안의 page를 안전하게 내보냄
```

## 6.2 기본 흐름

```text
1. victim frame을 고른다.
2. victim->page를 가져온다.
3. page 타입에 맞는 swap_out(page)을 호출한다.
4. page table에서 page->va 매핑을 제거한다.
5. page->frame을 NULL로 만든다.
6. victim->page를 NULL로 만든다.
7. victim frame을 반환한다.
```

## 6.3 코드 흐름 예시

```c
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	if (victim == NULL)
		return NULL;

	struct page *page = victim->page;
	if (page == NULL)
		return victim;

	if (!swap_out (page))
		return NULL;

	pml4_clear_page (thread_current ()->pml4, page->va);
	ASSERT (pml4_get_page (thread_current ()->pml4, page->va) == NULL);

	victim->page = NULL;
	page->frame = NULL;

	return victim;
}
```

## 6.4 왜 pml4 매핑을 지우는가

eviction 후에는 page가 더 이상 frame에 올라와 있지 않다.

그런데 page table에 `va -> old frame` 매핑이 남아 있으면 유저가 그 va로 계속 접근할 수 있다.

그러면 이미 다른 page가 재사용할 수도 있는 frame을 잘못 읽거나 쓰게 된다.

따라서 eviction 후에는 page table 매핑을 반드시 제거해야 한다.

```text
page는 SPT에는 남아 있음
page table 매핑은 제거됨
page->frame은 NULL

나중에 다시 접근하면 page fault
-> SPT에서 page 찾기
-> swap_in으로 복구
```

## 6.5 eviction은 page 삭제가 아니다

이 점이 중요하다.

eviction은 page를 없애는 작업이 아니다.

page는 SPT에 계속 남아 있어야 한다.

그래야 나중에 같은 va에 다시 접근했을 때 복구할 수 있다.

```text
spt_remove_page()
  page를 완전히 제거한다.

vm_evict_frame()
  page는 유지하고 frame만 빼앗는다.
```

---

# 7. Anonymous Swap

## 7.1 anon page란?

anonymous page는 파일 원본이 없는 page다.

예시는 다음과 같다.

```text
stack page
heap page
zero page
실행 중 만들어진 일반 메모리 page
```

anon page는 원본 파일이 없다.

그래서 eviction될 때 내용을 어딘가에 저장하지 않으면 복구할 수 없다.

그 저장 공간이 swap disk다.

## 7.2 swap disk

Pintos에서 swap disk는 별도 디스크로 제공된다.

보통 테스트 실행 시 다음과 같이 붙는다.

```bash
--swap-disk=4
```

커널에서는 다음과 같은 방식으로 swap disk를 가져온다.

```c
swap_disk = disk_get (1, 1);
```

프로젝트 환경에 따라 `disk_get()` 인자는 문서나 `devices/disk.c`를 확인해야 한다.

KAIST Pintos 계열에서는 보통 다음처럼 나뉜다.

```text
0:0 kernel disk
0:1 file system disk
1:0 scratch disk
1:1 swap disk
```

## 7.3 sector와 slot

디스크는 sector 단위로 읽고 쓴다.

일반적으로:

```text
DISK_SECTOR_SIZE = 512 bytes
PGSIZE = 4096 bytes
```

page 하나는 4096 bytes다.

따라서 page 하나를 저장하려면 sector 8개가 필요하다.

```text
PGSIZE / DISK_SECTOR_SIZE
= 4096 / 512
= 8 sectors
```

swap slot 하나는 page 하나를 저장할 수 있는 공간이다.

```text
1 slot = 1 page = 8 sectors
```

slot 번호를 sector 번호로 바꾸는 공식은 다음과 같다.

```c
sector = slot_idx * SECTORS_PER_PAGE;
```

예시:

```text
slot 0 -> sector 0~7
slot 1 -> sector 8~15
slot 2 -> sector 16~23
slot 3 -> sector 24~31
```

## 7.4 bitmap이 필요한 이유

swap disk의 어떤 slot이 사용 중인지 기억해야 한다.

이를 위해 bitmap을 사용한다.

```text
bit 0 == false
  slot 0 비어 있음

bit 0 == true
  slot 0 사용 중
```

필요한 전역 변수 예시는 다음과 같다.

```c
static struct disk *swap_disk;
static struct bitmap *swap_table;
```

`anon_page`에도 자신이 어느 slot에 저장되었는지 기억할 필드가 필요하다.

```c
struct anon_page {
	size_t slot_idx;
	bool is_swapped;
};
```

`is_swapped` 없이 `slot_idx`에 특별값을 넣는 방식도 가능하다.

예를 들어 `BITMAP_ERROR`를 slot 없음으로 볼 수 있다.

팀 코드 스타일에 맞춰 하나로 정하면 된다.

## 7.5 vm_anon_init

`vm_anon_init()`은 swap subsystem을 초기화한다.

해야 할 일은 다음과 같다.

```text
1. swap disk를 가져온다.
2. swap disk의 sector 수를 구한다.
3. page 하나당 필요한 sector 수를 계산한다.
4. slot 개수를 계산한다.
5. slot 개수만큼 bitmap을 만든다.
```

의사코드:

```c
#define SECTORS_PER_PAGE (PGSIZE / DISK_SECTOR_SIZE)

void
vm_anon_init (void) {
	swap_disk = disk_get (1, 1);
	if (swap_disk == NULL)
		PANIC ("swap disk not found");

	size_t slot_cnt = disk_size (swap_disk) / SECTORS_PER_PAGE;
	swap_table = bitmap_create (slot_cnt);
	if (swap_table == NULL)
		PANIC ("swap bitmap create failed");
}
```

## 7.6 anon_initializer

anon page가 처음 실제 anon page로 전환될 때 호출된다.

```c
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	page->operations = &anon_ops;
	page->anon.slot_idx = BITMAP_ERROR;
	page->anon.is_swapped = false;
	return true;
}
```

이 함수의 핵심은 다음이다.

```text
이 page는 이제 anon page다.
앞으로 swap_in, swap_out, destroy는 anon_ops를 따른다.
아직 swap disk에 나간 적은 없다.
```

## 7.7 anon_swap_out

anon page가 eviction될 때 호출된다.

해야 할 일:

```text
1. 빈 swap slot을 찾는다.
2. page->frame->kva에 있는 내용을 swap disk에 쓴다.
3. page->anon.slot_idx에 slot 번호를 저장한다.
4. page->anon.is_swapped = true로 표시한다.
```

의사코드:

```c
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon = &page->anon;
	void *kva = page->frame->kva;

	size_t slot = bitmap_scan_and_flip (swap_table, 0, 1, false);
	if (slot == BITMAP_ERROR)
		return false;

	for (size_t i = 0; i < SECTORS_PER_PAGE; i++) {
		disk_write (swap_disk,
				slot * SECTORS_PER_PAGE + i,
				(uint8_t *) kva + i * DISK_SECTOR_SIZE);
	}

	anon->slot_idx = slot;
	anon->is_swapped = true;
	return true;
}
```

주의:

`anon_swap_out()`은 page table 매핑을 지우는 함수가 아니다.

그 작업은 `vm_evict_frame()`에서 한다.

## 7.8 anon_swap_in

swap disk에 나가 있던 anon page가 다시 page fault로 들어올 때 호출된다.

해야 할 일:

```text
1. page->anon.slot_idx를 확인한다.
2. swap disk에서 해당 slot 내용을 frame->kva로 읽는다.
3. bitmap에서 해당 slot을 비움으로 표시한다.
4. page->anon.is_swapped = false로 바꾼다.
```

의사코드:

```c
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon = &page->anon;

	if (!anon->is_swapped)
		return true;

	size_t slot = anon->slot_idx;

	for (size_t i = 0; i < SECTORS_PER_PAGE; i++) {
		disk_read (swap_disk,
				slot * SECTORS_PER_PAGE + i,
				(uint8_t *) kva + i * DISK_SECTOR_SIZE);
	}

	bitmap_reset (swap_table, slot);
	anon->slot_idx = BITMAP_ERROR;
	anon->is_swapped = false;
	return true;
}
```

## 7.9 처음 만들어진 anon page의 swap_in

여기서 많이 헷갈린다.

anon page라고 해서 항상 swap disk에서 읽어오는 것은 아니다.

처음 만들어진 stack page는 swap disk에 나간 적이 없다.

이 경우에는 읽어올 slot이 없다.

그래서 `anon_swap_in()`은 다음 두 경우를 나눠야 한다.

```text
아직 swap_out된 적 없는 anon page
  swap disk에서 읽을 내용 없음
  그냥 true

swap_out된 적 있는 anon page
  swap slot에서 frame으로 읽어와야 함
```

---

# 8. File-backed Swap Out

## 8.1 file-backed page란?

file-backed page는 원본 파일을 backing store로 가지는 page다.

예시는 다음과 같다.

```text
mmap으로 만든 page
실행 파일 segment page
```

다만 팀 구현에 따라 실행 파일 segment는 anon으로 관리할 수도 있고 file로 관리할 수도 있다.

현재 흐름에서는 `VM_FILE`로 등록되는 부분이 있으므로 `file_backed_swap_out()`이 중요하다.

## 8.2 anon page와 다른 점

anon page는 원본 파일이 없다.

그래서 swap disk가 필요하다.

file-backed page는 원본 파일이 있다.

수정되지 않았다면 그냥 버려도 된다.

나중에 다시 필요하면 원본 파일에서 다시 읽을 수 있다.

하지만 수정되었다면 파일에 다시 써야 한다.

```text
dirty == false
  파일과 메모리 내용이 같음
  그냥 버려도 됨

dirty == true
  메모리 내용이 파일보다 최신임
  file_write_at으로 write-back 필요
```

## 8.3 dirty bit

dirty bit는 page가 수정되었는지 알려주는 bit다.

관련 함수:

```c
pml4_is_dirty (pml4, va);
pml4_set_dirty (pml4, va, false);
```

## 8.4 file_backed_swap_out 흐름

해야 할 일:

```text
1. page->file 정보 확인
2. dirty bit 확인
3. dirty이면 frame 내용을 파일에 write-back
4. dirty가 아니면 아무것도 쓰지 않음
5. 성공 여부 반환
```

의사코드:

```c
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;

	if (page->frame == NULL)
		return true;

	if (pml4_is_dirty (thread_current ()->pml4, page->va)) {
		void *buffer = page->frame->kva;
		off_t written = file_write_at (file_page->file,
				buffer,
				file_page->read_bytes,
				file_page->ofs);

		if (written != (off_t) file_page->read_bytes)
			return false;
	}

	return true;
}
```

주의:

`file_backed_swap_out()`도 page table 매핑을 지우는 함수가 아니다.

매핑 제거는 `vm_evict_frame()`의 책임이다.

## 8.5 file_backed_destroy와의 차이

`file_backed_swap_out()`은 eviction 때 호출된다.

page는 SPT에 남아 있어야 한다.

`file_backed_destroy()`는 page가 완전히 삭제될 때 호출된다.

예를 들면 `munmap`, process exit, SPT kill 같은 상황이다.

```text
file_backed_swap_out()
  page는 유지
  frame만 비움
  나중에 다시 page fault로 복구 가능

file_backed_destroy()
  page 자체를 제거
  SPT에서도 제거
  file close, frame free 등 최종 정리
```

---

# 9. Swap In과 Swap Out의 차이

## 9.1 swap_in

swap_in은 page를 frame으로 가져오는 작업이다.

호출되는 대표 상황:

```text
page fault
-> vm_do_claim_page()
-> frame 확보
-> pml4_set_page()
-> swap_in(page, frame->kva)
```

타입별 동작:

```text
UNINIT
  uninit_initialize()
  -> anon_initializer 또는 file_backed_initializer
  -> lazy_load_segment 또는 mmap_lazy_load

ANON
  anon_swap_in()
  -> swap slot에서 frame으로 읽기

FILE
  file_backed_swap_in()
  -> file에서 frame으로 읽기
```

## 9.2 swap_out

swap_out은 frame에 있는 page 내용을 밖으로 내보내는 작업이다.

호출되는 대표 상황:

```text
free frame 없음
-> vm_evict_frame()
-> swap_out(victim_page)
```

타입별 동작:

```text
ANON
  anon_swap_out()
  -> swap disk slot에 저장

FILE
  file_backed_swap_out()
  -> dirty이면 파일에 write-back
  -> dirty가 아니면 버림
```

## 9.3 매크로가 하는 일

`swap_in`과 `swap_out`은 매크로다.

```c
#define swap_in(page, v) (page)->operations->swap_in ((page), v)
#define swap_out(page) (page)->operations->swap_out (page)
```

이 매크로는 page 타입에 따라 맞는 함수를 호출해준다.

```text
page->operations == &anon_ops
  swap_out(page) -> anon_swap_out(page)

page->operations == &file_ops
  swap_out(page) -> file_backed_swap_out(page)
```

그래서 eviction 코드는 page 타입을 직접 if문으로 나누지 않아도 된다.

```c
swap_out (page);
```

이 한 줄로 타입별 함수가 자동 호출된다.

---

# 10. 함수별 역할 정리

## 10.1 vm_get_frame

역할:

```text
새 page에 줄 frame을 확보한다.
```

흐름:

```text
palloc_get_page(PAL_USER) 시도
-> 성공하면 새 frame 생성
-> 실패하면 vm_evict_frame()으로 기존 frame 재사용
```

주의:

eviction으로 받은 frame은 이미 frame_table에 있다.

## 10.2 vm_get_victim

역할:

```text
eviction할 frame을 고른다.
```

보통 second chance 방식 사용.

## 10.3 vm_evict_frame

역할:

```text
victim frame을 실제로 비워서 재사용 가능한 frame으로 만든다.
```

핵심 작업:

```text
swap_out(page)
pml4_clear_page()
page->frame = NULL
frame->page = NULL
```

## 10.4 anon_swap_out

역할:

```text
anon page 내용을 swap disk slot에 저장한다.
```

## 10.5 anon_swap_in

역할:

```text
swap disk slot에 있던 anon page 내용을 frame으로 복원한다.
```

## 10.6 file_backed_swap_out

역할:

```text
file-backed page가 수정되었으면 파일에 다시 쓴다.
```

## 10.7 file_backed_destroy

역할:

```text
file-backed page가 완전히 제거될 때 최종 정리한다.
```

`file_backed_swap_out()`과 헷갈리면 안 된다.

---

# 11. 구현 순서

Merge 4는 다음 순서가 가장 이해하기 쉽다.

## 11.1 1단계: frame_table 준비

```text
struct frame에 list_elem 확인
vm.c에 static struct list frame_table 선언
vm_init에서 list_init(&frame_table)
새 frame 생성 시 frame_table에 push
```

## 11.2 2단계: anon swap 자료구조 준비

```text
anon_page에 slot 정보 추가
anon.c에 swap_disk, swap_table 선언
vm_anon_init에서 swap disk와 bitmap 초기화
```

## 11.3 3단계: anon_swap_out 구현

```text
빈 slot 찾기
frame->kva 내용을 sector 단위로 disk_write
slot 번호를 page->anon에 저장
```

## 11.4 4단계: anon_swap_in 구현

```text
slot 번호 확인
sector 단위로 disk_read
bitmap slot 해제
page->anon slot 상태 초기화
```

## 11.5 5단계: file_backed_swap_out 구현

```text
dirty bit 확인
dirty이면 file_write_at
dirty 아니면 true
```

## 11.6 6단계: vm_get_victim 구현

```text
frame_table 순회
accessed bit 확인
second chance 적용
victim frame 반환
```

## 11.7 7단계: vm_evict_frame 구현

```text
victim 얻기
swap_out 호출
pml4_clear_page
page-frame 연결 해제
victim 반환
```

## 11.8 8단계: vm_get_frame과 연결

```text
palloc_get_page 실패 시 vm_evict_frame 호출
evicted frame을 새 page에게 반환
```

---

# 12. 자주 헷갈리는 지점

## 12.1 eviction은 page 삭제인가?

아니다.

eviction은 frame에서 page를 내보내는 것이다.

page는 SPT에 남아 있어야 한다.

## 12.2 swap_out이 pml4_clear_page까지 해야 하는가?

보통 아니다.

타입별 `swap_out()`은 내용을 백업하는 책임만 가진다.

page table 매핑 제거는 공통 eviction 흐름인 `vm_evict_frame()`에서 처리하는 것이 깔끔하다.

## 12.3 anon page는 항상 swap disk에서 읽는가?

아니다.

처음 만들어진 anon page는 swap disk에 저장된 적이 없다.

이 경우 `anon_swap_in()`은 그냥 true를 반환할 수 있다.

## 12.4 file-backed page도 swap disk에 쓰는가?

보통 아니다.

file-backed page는 원본 파일이 backing store다.

수정되지 않았으면 그냥 버리고, 수정되었으면 원본 파일에 write-back 한다.

## 12.5 dirty bit와 accessed bit는 다르다

```text
accessed bit
  최근 접근 여부
  victim selection에 사용

dirty bit
  수정 여부
  file-backed write-back 판단에 사용
```

## 12.6 frame을 free하는가, 재사용하는가?

eviction에서는 frame을 free하지 않는다.

비운 frame을 새 page에 재사용한다.

page destroy에서는 frame을 free할 수 있다.

두 상황을 구분해야 한다.

```text
eviction
  frame 재사용

destroy
  frame 반환
```

## 12.7 palloc_free_page는 언제 쓰는가?

page가 완전히 제거될 때 frame의 kva를 반환하기 위해 쓴다.

eviction으로 frame을 재사용할 때는 보통 `palloc_free_page(frame->kva)`를 하지 않는다.

## 12.8 같은 file을 page마다 close해도 되는가?

mmap에서 여러 page가 같은 `struct file *`을 공유한다면 page마다 close하면 위험하다.

한 mmap 영역 전체에서 file close 책임을 어떻게 둘지 팀 규약이 필요하다.

간단한 방법:

```text
첫 번째 mmap page에만 is_mmap_start = true
첫 번째 page에 page_cnt 저장
munmap은 첫 page 기준으로 page_cnt만큼 제거
file_close는 mmap 영역 단위로 한 번만 수행
```

---

# 13. 디버깅 체크포인트

## 13.1 palloc 실패가 실제로 eviction으로 이어지는가

`vm_get_frame()`에 찍어볼 수 있다.

```c
printf ("GET_FRAME: palloc failed, try eviction\n");
```

## 13.2 victim이 제대로 골라지는가

`vm_get_victim()`에서 확인한다.

```c
printf ("VICTIM: frame=%p page=%p va=%p\n",
		frame, frame->page, frame->page ? frame->page->va : NULL);
```

## 13.3 swap_out이 호출되는가

`vm_evict_frame()`에서 확인한다.

```c
printf ("EVICT: victim=%p page=%p va=%p type=%d\n",
		victim, page, page->va, page_get_type (page));
```

## 13.4 anon slot이 제대로 잡히는가

`anon_swap_out()`에서 확인한다.

```c
printf ("ANON_OUT: va=%p slot=%zu kva=%p\n",
		page->va, slot, page->frame->kva);
```

`anon_swap_in()`에서 확인한다.

```c
printf ("ANON_IN: va=%p slot=%zu kva=%p\n",
		page->va, anon->slot_idx, kva);
```

## 13.5 file-backed write-back이 되는가

`file_backed_swap_out()`에서 확인한다.

```c
printf ("FILE_OUT: va=%p dirty=%d ofs=%lld read=%zu\n",
		page->va,
		pml4_is_dirty (thread_current ()->pml4, page->va),
		page->file.ofs,
		page->file.read_bytes);
```

## 13.6 pml4 매핑이 제거되는가

`vm_evict_frame()`에서 확인한다.

```c
pml4_clear_page (thread_current ()->pml4, page->va);
printf ("EVICT_CLEAR: va=%p mapped=%p\n",
		page->va,
		pml4_get_page (thread_current ()->pml4, page->va));
```

정상이라면 `mapped == NULL`이어야 한다.

---

# 14. 테스트가 보는 것

## 14.1 swap-anon 계열

대략적으로 보는 것:

```text
메모리보다 많은 anon page를 사용한다.
frame이 부족해진다.
eviction이 발생한다.
anon page가 swap disk로 나간다.
나중에 다시 접근하면 swap disk에서 복원된다.
복원된 데이터가 이전과 같아야 한다.
```

검사 포인트:

```text
anon_swap_out이 slot에 제대로 쓰는가
anon_swap_in이 같은 slot에서 제대로 읽는가
slot bitmap을 해제하는가
page table 매핑을 제대로 지우는가
```

## 14.2 swap-file 계열

대략적으로 보는 것:

```text
mmap 또는 file-backed page를 사용한다.
frame이 부족해져 eviction이 발생한다.
수정된 page는 파일에 write-back 되어야 한다.
수정되지 않은 page는 다시 파일에서 읽으면 된다.
```

검사 포인트:

```text
dirty bit 확인을 하는가
dirty page만 file_write_at 하는가
write-back offset과 size가 맞는가
```

## 14.3 page-merge-stk 계열과 연결

stack page는 anon page다.

stack growth로 만들어진 page가 eviction되면 swap disk로 나가야 한다.

나중에 다시 stack 주소에 접근하면 swap_in으로 복원되어야 한다.

따라서 stack growth와 anon swap은 서로 연결된다.

---

# 15. Merge 4를 한 문장으로 설명하기

```text
Merge 4는 물리 메모리가 부족할 때 frame을 재사용하기 위해,
victim frame을 고르고, page 타입별로 내용을 안전하게 백업한 뒤,
page table 매핑과 page-frame 연결을 정리하는 과정이다.
```

---

# 16. 재혁님 기준 최종 이해 체크

아래 질문에 답할 수 있으면 merge4 swap의 큰 개념은 잡힌 것이다.

```text
1. eviction은 page 삭제인가, frame 재사용인가?
2. SPT에 page를 남겨야 하는 이유는 무엇인가?
3. anon page는 왜 swap disk가 필요한가?
4. file-backed page는 왜 dirty일 때만 write-back 하는가?
5. accessed bit와 dirty bit는 각각 어디에 쓰이는가?
6. swap_out이 끝난 뒤 pml4 매핑을 지워야 하는 이유는 무엇인가?
7. anon_swap_out에서 slot 번호를 page에 저장해야 하는 이유는 무엇인가?
8. anon_swap_in 후 bitmap slot을 비워야 하는 이유는 무엇인가?
9. evicted frame은 왜 palloc_free_page 하지 않고 재사용하는가?
10. file_backed_swap_out과 file_backed_destroy는 무엇이 다른가?
```

---

# 17. 아주 짧은 암기 버전

```text
frame 부족
-> victim frame 선택
-> victim page swap_out
   - anon: swap disk slot에 저장
   - file: dirty면 파일에 write-back
-> pml4 매핑 제거
-> page-frame 연결 해제
-> frame 재사용
-> 나중에 다시 접근하면 page fault
-> SPT에서 page 찾기
-> swap_in으로 복구
```

---

# 18. 시작 전 질문 답안 정리

## 18.1 Merge 4에서 eviction이 필요한 이유는 무엇인가?

물리 메모리의 유저 풀이 무한하지 않기 때문이다.

새 page를 frame에 올려야 하는데 `palloc_get_page(PAL_USER)`가 실패하면, 기존 frame 중 하나를 비워서 재사용해야 한다.

즉 eviction은 메모리가 부족할 때 VM이 계속 동작하기 위한 frame 확보 전략이다.

## 18.2 eviction은 page를 삭제하는 작업인가, frame을 재사용하는 작업인가?

eviction은 page 삭제가 아니라 frame 재사용 작업이다.

page는 SPT에 남겨두고, 그 page가 쓰던 frame만 비운다.

나중에 같은 va에 다시 접근하면 page fault가 발생하고, SPT에 남아 있던 page 정보를 이용해 다시 frame에 올릴 수 있다.

## 18.3 frame table은 왜 필요하고, 어떤 frame들이 들어가야 하는가?

frame table은 현재 메모리에 올라와 있는 frame 목록이다.

eviction할 victim frame을 고르려면 전체 frame 목록을 순회할 수 있어야 한다.

따라서 `palloc_get_page(PAL_USER)`로 새로 얻은 frame들이 frame table에 들어간다.

eviction으로 재사용한 frame은 이미 frame table에 들어 있으므로 다시 넣으면 안 된다.

## 18.4 victim frame은 어떤 기준으로 고를 수 있는가?

대표적으로 accessed bit를 이용한 second chance 방식을 쓸 수 있다.

최근에 접근된 page는 accessed bit가 켜져 있으므로 한 번 기회를 주고 bit를 false로 내린다.

accessed bit가 이미 false인 page는 최근에 안 쓰였다고 보고 victim으로 고른다.

## 18.5 accessed bit와 dirty bit는 각각 어디에 쓰이는가?

accessed bit는 최근 접근 여부를 판단하는 데 쓴다.

그래서 victim selection에서 사용한다.

dirty bit는 page가 수정되었는지 판단하는 데 쓴다.

그래서 file-backed page를 eviction할 때 파일에 write-back 해야 하는지 결정하는 데 사용한다.

```text
accessed bit -> victim 고르기
dirty bit    -> 파일에 다시 써야 하는지 판단
```

## 18.6 anon page가 eviction될 때 왜 swap disk가 필요한가?

anon page는 원본 파일이 없다.

stack이나 heap처럼 실행 중에 만들어진 데이터는 파일에서 다시 읽어올 수 없다.

따라서 eviction될 때 내용을 잃어버리지 않으려면 swap disk에 저장해야 한다.

## 18.7 swap slot 하나는 왜 page 하나와 같은 크기로 생각하는가?

eviction과 swap in/out은 page 단위로 일어난다.

Pintos의 page 크기는 보통 4096 bytes이고, disk sector는 보통 512 bytes다.

따라서 page 하나를 저장하려면 sector 8개가 필요하다.

그래서 swap slot 하나를 page 하나 크기, 즉 sector 8개 묶음으로 본다.

```text
1 page = 4096 bytes
1 sector = 512 bytes
1 swap slot = 8 sectors
```

## 18.8 anon_swap_out()은 정확히 무엇을 저장하고, 어디에 저장하는가?

`anon_swap_out()`은 현재 frame에 들어 있는 anon page의 실제 데이터를 swap disk의 빈 slot에 저장한다.

저장 대상은 `page->frame->kva`가 가리키는 page 크기 메모리 내용이다.

저장 후에는 page의 anon 정보에 slot 번호를 기록해야 한다.

그래야 나중에 `anon_swap_in()`이 어느 slot에서 읽어와야 하는지 알 수 있다.

## 18.9 anon_swap_in()은 언제 호출되고, 무엇을 복구하는가?

anon page가 eviction으로 swap disk에 나간 뒤, 유저가 다시 그 va에 접근하면 page fault가 난다.

그때 claim 과정에서 새 frame을 확보하고 `swap_in(page, frame->kva)`가 호출된다.

page가 anon 타입이면 `anon_swap_in()`이 실행되고, swap slot에 저장된 내용을 새 frame의 kva로 읽어온다.

## 18.10 file-backed page는 왜 dirty일 때만 파일에 write-back 하면 되는가?

file-backed page는 원본 파일이 backing store다.

수정되지 않은 page는 메모리 내용과 파일 내용이 같으므로 그냥 버려도 된다.

나중에 다시 필요하면 파일에서 다시 읽으면 된다.

하지만 dirty page는 메모리에서 수정된 내용이 파일에 아직 반영되지 않은 상태다.

따라서 dirty일 때만 `file_write_at()`으로 파일에 write-back 해야 한다.

## 18.11 vm_evict_frame()에서 swap_out 후 pml4_clear_page()를 해야 하는 이유는 무엇인가?

swap_out은 page 내용을 안전한 곳에 백업하는 작업이다.

하지만 page table 매핑을 지우지는 않는다.

eviction 후 frame은 다른 page가 재사용할 수 있다.

그런데 기존 va가 여전히 그 frame에 매핑되어 있으면, 유저가 evicted page에 접근했을 때 page fault가 나지 않고 잘못된 frame을 읽을 수 있다.

그래서 `pml4_clear_page()`로 기존 va와 frame의 매핑을 끊어야 한다.

## 18.12 eviction 후 page는 SPT에 남아 있어야 하는가?

남아 있어야 한다.

eviction은 page를 삭제하는 것이 아니라 frame에서 잠시 내보내는 것이다.

SPT에 page가 남아 있어야 나중에 같은 va에 접근했을 때 page fault handler가 그 page를 찾고 swap_in으로 복구할 수 있다.

## 18.13 vm_get_frame()에서 palloc_get_page()가 실패하면 어떤 흐름으로 넘어가야 하는가?

새 frame을 얻지 못한 상황이므로 기존 frame을 비워야 한다.

흐름은 다음과 같다.

```text
palloc_get_page(PAL_USER) 실패
-> vm_evict_frame()
-> vm_get_victim()
-> victim page swap_out
-> pml4 매핑 제거
-> page-frame 연결 해제
-> 비워진 frame 반환
```

그 frame을 새 page에게 연결해서 계속 진행한다.

## 18.14 file_backed_swap_out()과 file_backed_destroy()는 무엇이 다른가?

`file_backed_swap_out()`은 eviction 때 호출된다.

page는 SPT에 남아 있고, frame만 비워진다.

나중에 다시 접근하면 복구될 수 있어야 한다.

`file_backed_destroy()`는 page가 완전히 제거될 때 호출된다.

예를 들면 `munmap`, process exit, SPT kill 같은 상황이다.

이때는 page table 매핑, frame, file 자원 등을 최종 정리해야 한다.

```text
file_backed_swap_out
  page 유지
  frame만 비움

file_backed_destroy
  page 제거
  자원 최종 정리
```

## 18.15 swap_in과 swap_out은 왜 page 타입별 함수 포인터로 나뉘어 있는가?

page 타입마다 데이터를 복구하거나 내보내는 방법이 다르기 때문이다.

anon page는 swap disk를 사용한다.

file-backed page는 원본 파일을 사용한다.

uninit page는 먼저 실제 타입으로 초기화되어야 한다.

그래서 공통 코드에서는 `swap_in(page, kva)` 또는 `swap_out(page)`만 호출하고, 실제 동작은 `page->operations`에 연결된 타입별 함수가 처리한다.

```text
ANON page -> anon_swap_in / anon_swap_out
FILE page -> file_backed_swap_in / file_backed_swap_out
UNINIT page -> uninit_initialize
```
