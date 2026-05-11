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

## 4. 주소와 저장 위치를 먼저 구분하기

SPT를 이해할 때 가장 먼저 구분해야 하는 것은 “주소가 무엇을 가리키는지”와 “그 정보가 어디에 저장되는지”다.

### 4.1 유저 주소와 커널 주소

`va`는 virtual address, 즉 가상 주소다.  
그중에서도 SPT에서 key로 쓰는 주소는 **유저 가상 주소**다.

```text
user virtual address
- 유저 프로그램이 보는 주소
- 예: 유저 stack, code segment, data segment, mmap 영역
- SPT에서 page를 찾을 때 key로 사용
```

커널도 주소를 사용하지만, 커널이 쓰는 주소는 유저 프로그램의 주소와 다르다.

```text
kernel virtual address
- 커널이 메모리에 접근하기 위해 쓰는 주소
- palloc_get_page()가 반환하는 kva가 여기에 해당
- 유저 SPT에서 찾을 대상이 아님
```

그래서 `vm_try_handle_fault()`에서 이런 검사를 한다.

```c
if (addr == NULL || is_kernel_vaddr(addr)){
	return false;
}
```

이 검사는 page fault가 난 주소가 유저 영역 주소인지 확인하는 단계다.  
SPT는 유저 주소를 관리하므로, 커널 주소가 들어오면 복구 대상이 아니다.

### 4.2 page란 무엇인가

page는 유저 가상 주소 공간을 일정한 크기로 나눈 한 조각이다.

Pintos에서는 보통 page 하나가 4KB 크기다.  
유저 프로그램이 보는 code, data, stack, mmap 영역도 결국 page 단위로 관리된다.

```text
page
- 유저 가상 주소 공간의 한 조각
- page 시작 주소는 page-aligned address여야 함
- SPT에서는 va를 key로 struct page를 찾음
```

중요한 점은 `struct page`가 실제 데이터 자체는 아니라는 것이다.

```text
struct page
- 커널이 page를 관리하기 위해 만든 설명서
- 이 page의 va, 타입, writable 여부, 연결된 frame 정보를 저장
- SPT hash table에 등록됨
```

즉 page라는 말은 상황에 따라 두 가지 의미로 쓰인다.

```text
개념상 page
- 유저 가상 주소 공간의 4KB 단위 영역

코드상 struct page
- 그 page를 관리하기 위한 커널 구조체
```

### 4.3 page는 언제 만들어지는가

Project 3 VM에서는 page가 실제 메모리에 올라가는 시점보다 먼저 만들어진다.

보통 프로그램을 load할 때, ELF segment를 보면서 “이 가상 주소에는 이런 page가 필요하다”는 정보를 만든다.

```text
프로그램 load
-> segment 정보를 읽음
-> vm_alloc_page_with_initializer()
-> struct page malloc
-> uninit_new()
-> SPT에 등록
```

이 시점에 만들어지는 page는 대부분 `uninit page`다.

```text
page 생성 시점
- struct page가 만들어짐
- SPT에 등록됨
- page->va는 설정됨
- page->frame은 아직 NULL
- 실제 내용은 아직 물리 메모리에 없음
```

실제 frame이 붙는 시점은 나중이다.

```text
유저가 해당 va에 접근
-> page table에 매핑이 없어서 page fault 발생
-> SPT에서 page 찾음
-> vm_do_claim_page()
-> frame 할당
-> page와 frame 연결
-> page table 매핑
-> swap_in으로 실제 내용 로드
```

정리하면:

```text
page는 load 단계에서 미리 만들어져 SPT에 들어간다.
frame은 page fault가 발생해서 실제로 필요해질 때 연결된다.
```

### 4.4 page, frame, kva의 관계

개념적으로 page와 frame은 서로 다른 층에 있다.

```text
page
- 유저 가상 주소 공간의 한 페이지
- struct page로 관리
- SPT에 저장됨

frame
- 실제 물리 메모리의 page 크기 공간
- palloc_get_page(PAL_USER)로 확보
- 커널은 이 공간을 kva로 접근

struct frame
- 실제 frame을 관리하기 위한 커널 구조체
- frame->kva에 실제 frame의 커널 주소 저장
- frame->page에 연결된 struct page 저장
```

정리하면 다음과 같다.

```text
page->va
-> 유저가 접근한 가상 주소

frame->kva
-> 커널이 실제 물리 frame에 접근하기 위한 주소

pml4_set_page()
-> page->va와 frame->kva가 가리키는 물리 frame을 page table에 매핑
```

즉 `struct frame` 자체가 물리 메모리 공간은 아니다.  
`struct frame`은 물리 frame을 추적하기 위한 관리 구조체이고, 실제 물리 frame은 `frame->kva`가 가리킨다.

### 4.5 page table과 SPT의 차이

page table과 SPT는 둘 다 주소와 관련 있지만 역할이 다르다.

```text
page table
- CPU/MMU가 실제 주소 변환에 사용
- 현재 메모리에 올라온 va -> frame 매핑을 가짐
- pml4_set_page()로 매핑 추가

SPT
- 커널이 page fault를 복구하기 위해 참고
- 아직 메모리에 올라오지 않은 page 정보도 가짐
- va -> struct page를 저장
```

따라서 page fault가 났다는 것은 page table에 현재 매핑이 없거나 권한이 맞지 않다는 뜻이다.  
이때 SPT에 page가 있으면 “정상 page인데 아직 frame에 안 올라온 상태”일 수 있다.

### 4.6 stack은 어디에 있는가

stack도 유저 가상 주소 공간의 일부다.

```text
유저 stack
- 유저 프로그램의 함수 호출, 지역 변수, 인자 등이 저장되는 영역
- 높은 주소에서 낮은 주소 방향으로 자람
- 유저 주소이므로 SPT에서 page로 관리 가능
```

Project 2에서 argument passing을 할 때 문자열과 `argv`를 user stack에 넣었다.  
Project 3에서는 이 stack page도 필요할 때 frame에 올라오거나, 나중에는 stack growth로 새 page가 만들어질 수 있다.

즉 stack이라고 해서 특별히 SPT 밖에 있는 것이 아니다.  
stack page도 결국 유저 가상 주소의 page이고, 필요하면 SPT에 등록되어 frame과 매핑된다.

### 4.7 file-backed page와 offset

file-backed page는 파일을 원본으로 가지는 page다.

예를 들어 실행 파일의 code segment나 `mmap`으로 연결된 파일 page가 이에 해당한다.

이때 중요한 개념이 `offset`이다.

```text
offset
- 파일의 시작 지점으로부터 얼마나 떨어진 위치인지 나타내는 값
- 예: offset 0은 파일의 맨 앞
- 예: offset 4096은 파일 시작에서 4096바이트 뒤
```

file-backed page를 메모리에 올릴 때는 보통 이런 정보가 필요하다.

```text
어떤 파일에서 읽을지
파일의 몇 번째 위치부터 읽을지 offset
몇 바이트를 읽을지 read_bytes
남은 공간을 0으로 채울지 zero_bytes
쓰기 가능한 page인지 writable
```

lazy loading에서는 page를 처음 만들 때 파일 내용을 바로 읽지 않는다.  
대신 SPT의 uninit page 안에 “나중에 어떤 파일의 어느 offset부터 읽어야 하는지” 같은 정보를 저장해둔다.

이후 page fault가 발생하면:

```text
1. SPT에서 page를 찾는다.
2. frame을 확보한다.
3. page table에 매핑한다.
4. swap_in()을 호출한다.
5. file-backed page라면 저장해둔 file, offset 정보를 이용해 frame->kva에 파일 내용을 읽어온다.
```

### 4.8 어디에 저장되는지 한 번에 보기

```text
struct page
- 커널 메모리에 저장되는 관리 정보
- SPT hash table에 들어감
- va, frame, writable, operations, uninit/anon/file 정보를 가짐

struct frame
- 커널 메모리에 저장되는 관리 정보
- frame_table list에 들어감
- kva, page, elem 정보를 가짐

실제 page 내용
- user pool에서 palloc_get_page(PAL_USER)로 받은 물리 frame에 저장
- 커널은 frame->kva로 접근

page table
- CPU/MMU가 va를 실제 frame으로 변환할 때 사용
- pml4_set_page()로 va -> frame 매핑을 추가

SPT
- page fault 때 커널이 참고하는 보조 테이블
- va -> struct page를 저장
```

## 5. 전체 흐름

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

## 6. 구조체 변경

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

## 7. 함수 흐름별 코드

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

## 8. 핵심 문장

SPT는 page fault가 났을 때 “이 주소가 정말 잘못된 주소인지, 아니면 아직 메모리에 안 올라온 정상 page인지”를 판단하기 위한 자료구조다.

우리 구현에서는 SPT를 hash table로 만들었고, key는 page 시작 주소로 정렬된 user virtual address, value는 `struct page`로 잡았다.

page fault가 발생하면 `vm_try_handle_fault()`에서 주소와 권한을 검사하고, SPT에서 page를 찾은 뒤 `vm_do_claim_page()`로 넘겨 frame 연결과 page table 매핑을 수행한다.

## 9. 아직 남은 부분

현재 SPT 기본 흐름은 구현했지만, 완성도를 위해 남은 부분이 있다.

- `page_less` 비교식 확인
- `vm_do_claim_page()`에서 `swap_in()` 실패 시 정리
- `supplemental_page_table_copy()`
- `supplemental_page_table_kill()`
- eviction 관련 함수
  - `vm_get_victim()`
  - `vm_evict_frame()`
