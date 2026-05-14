# Pintos Project 3 VM - Page Type 정리

## 1. 이 문서의 목표

이 문서는 Pintos Project 3 VM에서 page 타입이 어떻게 정해지고, 언제 실제 타입으로 바뀌는지 정리한다.

특히 아래 질문에 답하는 것이 목표다.

- page는 어느 시점에 만들어지는가?
- page 타입에는 무엇이 있는가?
- SPT에 page를 넣을 때 타입은 어떻게 예약되는가?
- 실제 page 타입은 언제 바뀌는가?
- 타입 변경을 수행하는 함수는 무엇인가?
- `uninit`, `anon`, `file` page를 어떻게 구분해서 이해해야 하는가?

## 2. Page Type 큰 그림

Project 3에서 중요한 page 타입은 크게 세 가지다.

```text
VM_UNINIT
VM_ANON
VM_FILE
```

각 타입의 의미는 다음과 같다.

```text
VM_UNINIT
- 아직 실제 타입으로 초기화되지 않은 page
- lazy loading을 위해 잠시 대기하는 상태
- "나중에 anon 또는 file page가 될 예정"인 page

VM_ANON
- anonymous page
- 파일과 계속 연결되어 있지 않은 page
- stack, heap, 실행 파일의 일반 segment 등이 여기에 해당할 수 있다

VM_FILE
- file-backed page
- mmap처럼 파일과 계속 연결되는 page
- 나중에 dirty page write-back이 필요할 수 있다
```

가장 중요한 문장은 이것이다.

```text
대부분의 page는 처음에는 VM_UNINIT으로 만들어지고,
처음 실제로 필요해지는 순간 VM_ANON 또는 VM_FILE로 바뀐다.
```

## 3. Page는 언제 만들어지는가?

page는 유저 프로그램이 실행 파일을 읽거나, 스택을 준비하거나, mmap을 등록할 때 만들어진다.

대표 흐름은 다음과 같다.

```text
실행 파일 segment 등록
load_segment()
-> vm_alloc_page_with_initializer()
-> malloc(sizeof *page)
-> uninit_new()
-> spt_insert_page()

초기 stack page 등록
setup_stack()
-> vm_alloc_page(VM_ANON, stack_bottom, true)
-> vm_alloc_page_with_initializer()
-> malloc(sizeof *page)
-> uninit_new()
-> spt_insert_page()

mmap page 등록
do_mmap()
-> vm_alloc_page_with_initializer(VM_FILE, ...)
-> malloc(sizeof *page)
-> uninit_new()
-> spt_insert_page()
```

여기서 중요한 점은 `struct page`는 만들어지지만, 아직 실제 frame이 붙지 않았을 수 있다는 점이다.

```text
SPT에 page 등록
-> page 정보만 있음
-> frame은 아직 없을 수 있음
-> page table 매핑도 아직 없을 수 있음
```

## 4. SPT에 넣을 때 실제 타입으로 바로 바뀌는가?

아니다.

SPT에 넣을 때는 보통 실제 타입으로 바로 바뀌지 않는다.

SPT에 넣을 때는 다음 정보들을 저장해둔다.

```text
va
writable
나중에 될 타입
초기화 함수
초기화에 필요한 보조 정보 aux
```

즉 SPT에 들어간 page는 이런 느낌이다.

```text
"나는 지금은 uninit page다.
하지만 나중에 처음 접근되면 VM_ANON 또는 VM_FILE로 바뀔 예정이다."
```

이 "나중에 될 타입"은 `struct uninit_page` 안에 저장된다.

```c
struct uninit_page {
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};
```

여기서 핵심 필드는 다음과 같다.

```text
type
-> 나중에 바뀔 예정 타입
-> VM_ANON 또는 VM_FILE

init
-> 실제 내용을 채울 때 추가로 실행할 함수
-> 예: lazy_load_segment

aux
-> init 함수에 넘길 보조 정보
-> 예: file, offset, read_bytes, zero_bytes

page_initializer
-> page를 실제 타입으로 바꾸는 함수
-> 예: anon_initializer, file_backed_initializer
```

## 5. uninit_new()는 무엇을 하는가?

`uninit_new()`는 page를 처음 `VM_UNINIT` 상태로 만든다.

위치는 다음 파일에 있다.

```text
pintos/vm/uninit.c
```

핵심 동작은 다음과 같다.

```text
page->operations = &uninit_ops
page->va = va
page->frame = NULL
page->uninit.init = init
page->uninit.type = type
page->uninit.aux = aux
page->uninit.page_initializer = initializer
```

즉 `uninit_new()`는 page를 실제 anon/file page로 만드는 함수가 아니다.

`uninit_new()`의 역할은 다음과 같다.

```text
page를 uninit 상태로 만들고,
나중에 어떤 타입으로 바뀔지 예약 정보를 저장한다.
```

## 6. page 타입은 언제 실제로 바뀌는가?

page 타입은 해당 page가 처음 실제로 필요해질 때 바뀐다.

대표적으로 page fault가 발생하거나, `vm_claim_page()`를 통해 즉시 claim할 때다.

흐름은 다음과 같다.

```text
유저가 va에 접근
-> page table에 매핑 없음
-> page fault
-> SPT에서 page 찾기
-> vm_do_claim_page(page)
-> frame 확보
-> page와 frame 연결
-> pml4_set_page()
-> swap_in(page, frame->kva)
-> uninit_initialize(page, frame->kva)
-> anon_initializer() 또는 file_backed_initializer()
```

타입 변경이 실제로 일어나는 지점은 아래 호출이다.

```c
uninit->page_initializer(page, uninit->type, kva)
```

이 호출이 `anon_initializer()` 또는 `file_backed_initializer()`로 이어진다.

## 7. uninit_initialize()는 무엇을 하는가?

`uninit_initialize()`는 uninit page를 실제 타입의 page로 바꾸는 중간 관리자다.

위치는 다음 파일에 있다.

```text
pintos/vm/uninit.c
```

현재 흐름은 다음과 같다.

```text
uninit page가 swap_in 됨
-> uninit_initialize(page, kva) 호출
-> uninit에 저장되어 있던 init, aux, type을 꺼냄
-> page_initializer(page, type, kva) 호출
-> init(page, aux)가 있으면 추가 실행
```

코드 흐름은 다음과 같다.

```c
return uninit->page_initializer(page, uninit->type, kva) &&
	(init ? init(page, aux) : true);
```

이 코드는 두 단계를 의미한다.

```text
1. page_initializer 실행
   -> page를 VM_ANON 또는 VM_FILE 타입으로 바꿈

2. init 실행
   -> 실제 내용을 frame에 채움
   -> 예: lazy_load_segment가 파일 내용을 frame->kva에 읽음
```

## 8. anon_initializer()는 무엇을 하는가?

`anon_initializer()`는 page를 anonymous page로 바꾼다.

위치는 다음 파일에 있다.

```text
pintos/vm/anon.c
```

핵심 동작은 다음과 같다.

```c
page->operations = &anon_ops;
```

이 한 줄이 중요하다.

`page->operations`가 `anon_ops`로 바뀌면, 이 page는 이제 anonymous page처럼 동작한다.

이후 이 page에 대해 `swap_in`, `swap_out`, `destroy`를 호출하면 anon용 함수가 실행된다.

```text
swap_in(page, kva)
-> anon_swap_in(page, kva)

swap_out(page)
-> anon_swap_out(page)

destroy(page)
-> anon_destroy(page)
```

정리하면 다음과 같다.

```text
anon_initializer()
-> uninit page를 anon page로 바꿈
-> page->operations를 anon_ops로 교체
```

## 9. file_backed_initializer()는 무엇을 하는가?

`file_backed_initializer()`는 page를 file-backed page로 바꾼다.

위치는 다음 파일에 있다.

```text
pintos/vm/file.c
```

핵심 동작은 다음과 같다.

```c
page->operations = &file_ops;
```

`page->operations`가 `file_ops`로 바뀌면, 이 page는 이제 file-backed page처럼 동작한다.

이후 이 page에 대해 `swap_in`, `swap_out`, `destroy`를 호출하면 file용 함수가 실행된다.

```text
swap_in(page, kva)
-> file_backed_swap_in(page, kva)

swap_out(page)
-> file_backed_swap_out(page)

destroy(page)
-> file_backed_destroy(page)
```

정리하면 다음과 같다.

```text
file_backed_initializer()
-> uninit page를 file page로 바꿈
-> page->operations를 file_ops로 교체
```

## 10. page_get_type()은 왜 필요한가?

`page_get_type()`은 page의 현재 타입 또는 예정 타입을 확인하기 위한 함수다.

위치는 다음 파일에 있다.

```text
pintos/vm/vm.c
```

중요한 점은 `VM_UNINIT` page의 경우다.

uninit page는 현재 `page->operations->type`만 보면 `VM_UNINIT`이다.

하지만 실제로 알고 싶은 것은 "나중에 어떤 타입이 될 page인가?"일 때가 많다.

그래서 `page_get_type()`은 다음처럼 동작한다.

```text
현재 operations type이 VM_UNINIT이면
-> page->uninit.type을 반환

그 외에는
-> 현재 operations type을 반환
```

즉 uninit page에 대해 `page_get_type()`을 호출하면 현재 타입 `VM_UNINIT`이 아니라 예정 타입 `VM_ANON` 또는 `VM_FILE`을 돌려줄 수 있다.

## 11. 실행 파일 segment는 왜 VM_ANON인가?

실행 파일에서 내용을 읽어오는데도 `VM_ANON`을 쓰는 경우가 있다.

이유는 실행 파일 segment와 mmap file-backed page의 성격이 다르기 때문이다.

```text
실행 파일 segment
- 처음 내용은 실행 파일에서 읽어옴
- 하지만 실행 중 수정된 내용을 원본 실행 파일에 write-back하지 않음
- 초기화 이후에는 일반 anonymous page처럼 취급 가능

mmap file-backed page
- 파일과 계속 연결됨
- dirty 상태라면 munmap 또는 eviction 때 파일에 write-back해야 함
- VM_FILE로 다루는 것이 자연스러움
```

그래서 실행 파일 segment lazy loading은 보통 다음처럼 이해한다.

```text
처음 내용은 file에서 읽는다
하지만 page의 최종 성격은 anonymous page다
```

반면 mmap은 다음처럼 이해한다.

```text
처음부터 끝까지 파일과 연결된 page다
```

## 12. 타입 예약과 실제 타입 변경 흐름

전체 흐름을 한 번에 보면 다음과 같다.

```text
1. page 등록 요청
   vm_alloc_page_with_initializer(type, upage, writable, init, aux)

2. struct page 생성
   malloc(sizeof *page)

3. uninit page로 초기화
   uninit_new(page, upage, init, type, aux, initializer)

4. SPT에 삽입
   spt_insert_page(spt, page)

5. 아직 frame 없음
   page->frame == NULL

6. 나중에 접근 발생
   page fault 또는 vm_claim_page()

7. frame 확보
   vm_get_frame()

8. page와 frame 연결
   page->frame = frame
   frame->page = page

9. page table 매핑
   pml4_set_page()

10. swap_in 호출
    swap_in(page, frame->kva)

11. page가 uninit이면
    uninit_initialize(page, frame->kva)

12. 실제 타입으로 전환
    anon_initializer() 또는 file_backed_initializer()

13. 필요한 실제 내용 채우기
    lazy_load_segment() 또는 anon/file swap_in
```

## 13. 현재 타입과 예정 타입을 구분해야 한다

초보자가 가장 많이 헷갈리는 부분은 이것이다.

```text
현재 타입
-> 지금 page->operations가 가리키는 타입

예정 타입
-> uninit page 안에 저장된, 나중에 바뀔 타입
```

예를 들어 stack page를 등록했다고 해보자.

```c
vm_alloc_page(VM_ANON, stack_bottom, true);
```

이 호출은 내부적으로 다음처럼 된다.

```text
type = VM_ANON
init = NULL
aux = NULL
```

하지만 이 page가 SPT에 들어간 직후 바로 anon page인 것은 아니다.

정확히는 다음 상태다.

```text
현재 타입: VM_UNINIT
예정 타입: VM_ANON
```

이후 `vm_claim_page(stack_bottom)`이 실행되고 `swap_in()`까지 도달하면 `uninit_initialize()`가 실행된다.

그때 `anon_initializer()`가 호출되어 실제 anon page가 된다.

## 14. 어떤 함수가 무엇을 담당하는가?

```text
vm_alloc_page_with_initializer()
- page 생성 흐름의 입구
- 타입, writable, init, aux를 받음
- page를 만들고 uninit_new()를 호출해야 함
- 마지막에 SPT에 넣음

uninit_new()
- page를 VM_UNINIT 상태로 초기화
- 나중에 될 타입과 init/aux 저장

spt_insert_page()
- page를 SPT hash table에 등록

vm_claim_page()
- va로 SPT에서 page를 찾음
- 찾은 page를 vm_do_claim_page()로 넘김

vm_do_claim_page()
- frame 확보
- page와 frame 연결
- page table 매핑
- swap_in 호출

swap_in()
- page->operations->swap_in 호출
- page가 uninit이면 uninit_initialize()가 실행됨

uninit_initialize()
- uninit page를 실제 타입으로 바꾸는 중간 관리자
- page_initializer를 호출함
- init이 있으면 init도 호출함

anon_initializer()
- page->operations를 anon_ops로 바꿈
- page를 anon page로 전환

file_backed_initializer()
- page->operations를 file_ops로 바꿈
- page를 file page로 전환

lazy_load_segment()
- 실행 파일 segment 내용을 frame->kva에 실제로 읽음
- page 타입을 바꾸는 함수가 아니라, frame에 내용을 채우는 함수
```

## 15. 반드시 알아야 할 추가 개념

### 15.1 page 타입은 enum 값만으로 정해지는 것이 아니다

`VM_ANON`, `VM_FILE` 같은 enum 값도 중요하지만, 실제 동작을 결정하는 것은 `page->operations`다.

```text
page->operations = &uninit_ops
-> uninit page처럼 동작

page->operations = &anon_ops
-> anon page처럼 동작

page->operations = &file_ops
-> file page처럼 동작
```

즉 실제 타입 전환은 `page->operations`가 바뀌는 순간이라고 봐도 된다.

### 15.2 union은 타입별 데이터를 한 공간에 담기 위한 구조다

`struct page` 안에는 union이 있다.

```c
union {
	struct uninit_page uninit;
	struct anon_page anon;
	struct file_page file;
};
```

이 말은 page 하나가 상황에 따라 다음 중 하나의 타입별 정보를 가진다는 뜻이다.

```text
uninit 정보
anon 정보
file 정보
```

처음에는 `page->uninit`을 사용하고, 실제 타입으로 바뀐 뒤에는 `page->anon` 또는 `page->file`을 사용한다.

### 15.3 lazy loading은 타입 변경과 내용 채우기를 나눠서 봐야 한다

타입 변경:

```text
uninit_initialize()
-> anon_initializer() 또는 file_backed_initializer()
```

내용 채우기:

```text
lazy_load_segment()
anon_swap_in()
file_backed_swap_in()
```

두 개를 섞어서 생각하면 헷갈린다.

```text
타입 변경 = 이 page가 앞으로 어떤 종류의 page처럼 동작할지 결정
내용 채우기 = frame 안에 실제 바이트를 넣는 작업
```

## 16. 한 문장 요약

Pintos VM에서 page는 보통 처음 `VM_UNINIT`으로 SPT에 등록되고, 첫 접근 또는 claim 시 `swap_in()`을 거쳐 `uninit_initialize()`가 실행되면서 `anon_initializer()` 또는 `file_backed_initializer()`에 의해 실제 `VM_ANON` 또는 `VM_FILE` page로 전환된다.

