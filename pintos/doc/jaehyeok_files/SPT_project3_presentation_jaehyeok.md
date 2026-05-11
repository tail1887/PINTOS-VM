# Pintos Project 3 VM - SPT

## 1. SPT가 무엇인가

SPT는 Supplemental Page Table의 약자다.

기본 page table은 현재 메모리에 올라와 있는 `va -> pa` 매핑을 관리한다.  
반면 SPT는 아직 메모리에 올라오지 않은 page까지 포함해서, 각 유저 가상 주소에 해당하는 page 정보를 커널이 기억하기 위한 자료구조다.

즉 SPT는 단순한 주소 변환표라기보다, page fault가 났을 때 커널이 참고하는 page 설명서에 가깝다.

SPT에는 보통 이런 정보가 들어간다.

- 이 page의 유저 가상 주소 `va`
- 이 page가 어떤 타입인지 `uninit`, `anon`, `file`
- 쓰기 가능한 page인지 `writable`
- 아직 frame과 연결되어 있는지
- page fault 때 어떤 방식으로 내용을 메모리에 올릴지

이번 구현에서는 SPT를 hash table로 만들었다.

```text
key   : page 시작 주소로 정렬된 user virtual address
value : struct page
```

## 2. SPT는 언제 사용되는가

SPT는 주로 page fault가 났을 때 사용된다.

유저 프로그램이 어떤 주소에 접근했는데, 현재 page table에 그 주소가 매핑되어 있지 않으면 page fault가 발생한다.

이때 커널은 바로 잘못된 접근이라고 판단하지 않고, 먼저 SPT를 확인한다.

```text
1. 유저가 va에 접근한다.
2. page table에 va 매핑이 없으면 page fault가 발생한다.
3. 커널이 SPT에서 va에 해당하는 struct page를 찾는다.
4. SPT에 page가 없으면 잘못된 접근이다.
5. SPT에 page가 있으면 정상 page일 수 있다.
6. frame을 할당하고 page table에 va -> frame을 매핑한다.
7. swap_in으로 실제 내용을 frame에 올린다.
```

즉 SPT는 “이 주소가 진짜 없는 주소인지, 아니면 아직 메모리에 안 올라온 정상 page인지”를 판단하는 기준이다.

## 3. Page Fault가 무엇인가

Page fault는 CPU가 어떤 가상 주소에 접근하려고 했는데, page table을 통해 정상적으로 접근할 수 없을 때 발생하는 예외다.

대표적인 경우는 다음과 같다.

- 해당 `va`가 page table에 매핑되어 있지 않다.
- read-only page에 write를 시도했다.
- 유저 프로그램이 커널 주소에 접근하려 했다.
- 아예 SPT에도 등록되지 않은 잘못된 주소에 접근했다.

Project 3 VM에서는 page fault를 단순 에러로만 보지 않는다.  
정상적인 lazy loading 상황에서는 page fault가 발생한 뒤, SPT를 참고해서 page를 실제 frame에 올려 복구한다.

## 4. 전체 흐름

현재 구현 흐름을 함수 기준으로 정리하면 다음과 같다.

```text
vm_alloc_page_with_initializer()
-> uninit page 생성
-> SPT에 page 등록

page fault 발생
-> vm_try_handle_fault()
-> SPT에서 addr에 해당하는 page 탐색
-> 권한 검사
-> vm_do_claim_page()
-> vm_get_frame()
-> page와 frame 연결
-> pml4_set_page()로 page table 매핑
-> swap_in()
```

## 5. 구조체 변경

파일 위치:

```text
C:\jungle4\week11\pintos\include\vm\vm.h
```

### struct page

```c
struct page {
	const struct page_operations *operations;
	void *va;              /* Address in terms of user space */
	struct frame *frame;   /* Back reference for frame */
	struct hash_elem elem;
	bool writable;
	/* Your implementation */

	/* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
	union {
		struct uninit_page uninit;
		struct anon_page anon;
		struct file_page file;
#ifdef EFILESYS
		struct page_cache page_cache;
#endif
	};
};
```

핵심 추가 내용:

- `struct hash_elem elem`
  - SPT hash table에 page를 넣기 위한 연결고리
- `bool writable`
  - 이 page가 write 가능한 page인지 저장

### struct supplemental_page_table

```c
struct supplemental_page_table {
	struct hash hash;
};
```

핵심 추가 내용:

- 각 thread가 자기 SPT를 가지고 있고, 그 안에 hash table을 가진다.

## 6. 함수 흐름별 코드

아래 코드는 현재 로컬에 작성된 코드를 기준으로 그대로 정리한 것이다.

### 6.1 SPT 초기화

역할:

현재 thread의 SPT hash table을 초기화한다.

```c
/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->hash, page_hash, page_less, NULL); 
}
```

흐름:

```text
1. spt 안의 hash를 초기화한다.
2. page_hash를 hash 함수로 등록한다.
3. page_less를 비교 함수로 등록한다.
```

### 6.2 page_hash

역할:

page의 `va`를 기준으로 hash bucket을 고른다.

```c
static unsigned
page_hash(const struct hash_elem *e, void *aux){
	UNUSED(aux);
	struct page *pe = hash_entry(e, struct page, elem);
	return hash_bytes(&pe->va, sizeof pe->va);
}
```

흐름:

```text
1. hash_elem을 struct page 포인터로 복원한다.
2. page->va 값을 기준으로 hash 값을 만든다.
3. 이 hash 값으로 bucket 위치가 결정된다.
```

### 6.3 page_less

역할:

같은 bucket 안에서 page들을 비교할 기준을 정한다.

```c
bool
page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	UNUSED(aux);
	struct page *pa = hash_entry(a, struct page, elem);
	struct page *pb = hash_entry(b, struct page, elem);
	return pb->va < pb->va;
}
```

흐름:

```text
1. hash_elem a를 struct page *pa로 복원한다.
2. hash_elem b를 struct page *pb로 복원한다.
3. 두 page의 va를 비교해서 순서를 정한다.
```

주의:

현재 로컬 코드 그대로 적었지만, 발표 전에 비교식은 다시 확인이 필요하다.

### 6.4 vm_alloc_page_with_initializer

역할:

새 uninit page를 만들고 SPT에 등록한다.

```c
 bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)
	struct supplemental_page_table *spt = &thread_current ()->spt;
	upage=pg_round_down(upage);
	
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		bool (*initializer)(struct page *, enum vm_type, void *) = NULL;
		if (VM_TYPE(type) == VM_ANON){
			initializer = anon_initializer;
		} else if (VM_TYPE(type) == VM_FILE) {
			initializer = file_backed_initializer;
		}
		struct page *page = malloc(sizeof *page);
		if (page == NULL){
			return false;
		}
		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;

		if (spt_insert_page(spt, page)){
			return true;
		}
		free(page);
	}

err:
	return false;
}
```

흐름:

```text
1. type이 VM_UNINIT이 아닌지 확인한다.
2. 현재 thread의 SPT를 가져온다.
3. upage를 page 시작 주소로 맞춘다.
4. 같은 upage의 page가 SPT에 이미 있는지 확인한다.
5. 없으면 type에 맞는 initializer를 고른다.
6. struct page를 malloc한다.
7. uninit_new()로 uninit page 정보를 채운다.
8. writable 정보를 저장한다.
9. SPT에 page를 등록한다.
10. 성공하면 true, 실패하면 page를 free하고 false를 반환한다.
```

### 6.5 spt_insert_page

역할:

page를 SPT hash table에 등록한다.

```c
/* Insert PAGE into spt with validation. */
// SPT에 page의 hash_elem을 넣고, 같은 va의 page가 이미 있으면 false를 반환한다.
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	bool succ = false;
	struct hash_elem *result = hash_insert(&spt->hash, &page->elem);
	if (result == NULL){
		succ=true;
	}
	return succ;
}
```

흐름:

```text
1. page의 elem을 SPT hash에 넣는다.
2. 같은 va를 가진 page가 이미 있으면 hash_insert가 기존 elem을 반환한다.
3. 새로 삽입에 성공하면 NULL을 반환한다.
4. result가 NULL이면 true, 아니면 false를 반환한다.
```

### 6.6 spt_find_page

역할:

임의의 va를 받아 SPT에서 해당 page를 찾는다.

```c
/* Find VA from spt and return page. On error, return NULL. */
// va를 page 시작 주소로 맞추고, SPT에서 해당 page를 찾아 반환한다.
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va ) {
	va = pg_round_down(va);
	struct page temp;
	temp.va = va;
	struct hash_elem *e = hash_find(&spt->hash, &temp.elem);
	if (e != NULL){
		struct page *pe = hash_entry(e, struct page, elem);
		return pe;
	}
	return NULL;
}
```

흐름:

```text
1. va를 page 시작 주소로 내린다.
2. 임시 struct page temp를 만든다.
3. temp.va에 찾고 싶은 va를 넣는다.
4. temp.elem을 key처럼 사용해서 hash_find를 호출한다.
5. 찾으면 hash_entry로 실제 struct page를 꺼내 반환한다.
6. 못 찾으면 NULL을 반환한다.
```

### 6.7 spt_remove_page

역할:

SPT에서 page를 제거하고 page를 해제한다.

```c
// SPT hash에서 page의 elem을 제거하고 page를 해제한다.
void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->hash, &page->elem);
	vm_dealloc_page (page);	
}
```

흐름:

```text
1. hash_delete로 SPT hash에서 page의 elem을 제거한다.
2. vm_dealloc_page(page)로 page를 해제한다.
```

### 6.8 vm_try_handle_fault

역할:

page fault가 복구 가능한지 검사하고, 가능하면 page를 claim한다.

```c
/* Return true on success */
// page fault 주소를 검사하고, SPT에서 찾은 page를 claim해 복구를 시도한다.
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr ,
		bool user UNUSED, bool write , bool not_present ) {
	struct supplemental_page_table *spt  = &thread_current ()->spt;
	if (addr == NULL || is_kernel_vaddr(addr)){
		return false;
	}
	if (!not_present) {
		return false;
	}
	struct page *page = spt_find_page(spt, addr);
	if (page == NULL){
		return false;
	}
	if (write && !page->writable){
		return false;
	}
	return vm_do_claim_page (page);
}
```

흐름:

```text
1. 현재 thread의 SPT를 가져온다.
2. fault 주소 addr가 NULL이거나 kernel 주소면 false.
3. not_present fault가 아니면 false.
4. SPT에서 addr에 해당하는 page를 찾는다.
5. page가 없으면 false.
6. write fault인데 page가 writable이 아니면 false.
7. 통과하면 vm_do_claim_page(page)로 실제 복구를 시도한다.
```

### 6.9 vm_claim_page

역할:

va로 SPT에서 page를 찾고, 해당 page를 claim한다.

```c
/* Claim the page that allocate on VA. */
// SPT에서 page를 찾고, vm_do_claim_page에 전달하는 함수
bool
vm_claim_page (void *va) {
	if (va == NULL || is_kernel_vaddr(va)){
		return false;
	}
	struct page *page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL){
		return false;
	}
	return vm_do_claim_page (page);
}
```

흐름:

```text
1. va가 NULL이거나 kernel 주소면 false.
2. 현재 thread의 SPT에서 va에 해당하는 page를 찾는다.
3. page가 없으면 false.
4. page가 있으면 vm_do_claim_page(page)의 결과를 반환한다.
```

### 6.10 vm_do_claim_page

역할:

SPT에서 찾은 page에 frame을 연결하고 page table에 매핑한다.

```c
/* Claim the PAGE and set up the mmu. */
// page에 frame을 할당하고 page table에 매핑한 뒤 내용을 메모리에 올린다.
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (frame==NULL){
		return false;
	}
	/* Set links */
	frame->page = page;
	page->frame = frame;
	bool success = pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);
	if (!success){
		frame->page = NULL;
		page->frame = NULL;
		list_remove(&frame->elem);
		palloc_free_page(frame->kva);
		free(frame);
		return false;
	}

	return swap_in (page, frame->kva);
}
```

흐름:

```text
1. vm_get_frame()으로 frame을 하나 가져온다.
2. frame이 없으면 false.
3. frame->page = page로 frame이 page를 가리키게 한다.
4. page->frame = frame으로 page가 frame을 가리키게 한다.
5. pml4_set_page()로 page->va와 frame->kva를 page table에 매핑한다.
6. 매핑 실패 시 연결을 되돌리고 frame 자원을 정리한다.
7. 매핑 성공 시 swap_in(page, frame->kva)로 page 내용을 frame에 올린다.
```

## 7. 핵심 문장

SPT는 page fault가 났을 때 “이 주소가 정말 잘못된 주소인지, 아니면 아직 메모리에 안 올라온 정상 page인지”를 판단하기 위한 자료구조다.

우리 구현에서는 SPT를 hash table로 만들었고, key는 page 시작 주소로 정렬된 user virtual address, value는 `struct page`로 잡았다.

page fault가 발생하면 `vm_try_handle_fault()`에서 주소와 권한을 검사하고, SPT에서 page를 찾은 뒤 `vm_do_claim_page()`로 넘겨 frame 연결과 page table 매핑을 수행한다.

## 8. 아직 남은 부분

현재 SPT 기본 흐름은 구현했지만, 완성도를 위해 남은 부분이 있다.

- `page_less` 비교식 확인
- `vm_do_claim_page()`에서 `swap_in()` 실패 시 정리
- `supplemental_page_table_copy()`
- `supplemental_page_table_kill()`
- eviction 관련 함수
  - `vm_get_victim()`
  - `vm_evict_frame()`
