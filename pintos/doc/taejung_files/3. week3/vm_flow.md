### Pintos VM 통합 핸들링 흐름도 (End-to-End)

이 문서는 page fault가 발생했을 때 Pintos VM 계층이 어떤 판단을 하고,
어떤 자료구조를 갱신하며, 최종적으로 사용자 프로그램을 계속 실행시키는지
따라가기 위한 흐름도이다.

핵심은 다음 한 줄이다.

```text
CPU 접근 실패
-> page_fault()
-> vm_try_handle_fault()
-> SPT 조회
-> frame 확보
-> page 타입별 데이터 적재
-> PML4 매핑
-> fault 난 명령 재실행
```

---

## 0. 먼저 알아야 할 VM 요소들

VM 흐름도에 나오는 이름들은 대부분 "상태를 나타내는 값"이거나
"그 상태를 처리하는 함수"이다. 아래 요소들을 먼저 잡고 보면 뒤의 흐름이
훨씬 덜 헷갈린다.

```text
[page]
가상 주소 공간의 한 페이지를 나타내는 소프트웨어 객체.

예:
  사용자 프로그램이 0x8048000에 접근한다.
  -> SPT에서 0x8048000에 해당하는 struct page를 찾는다.

page가 기억하는 것:
  - 이 가상 주소가 합법적인가?
  - 이 page는 writable인가?
  - 현재 물리 frame에 올라와 있는가?
  - 아직 lazy load 전인가?
  - swap disk에 나가 있는가?
  - file에서 다시 읽어야 하는가?
```

```text
[frame]
실제 물리 메모리 한 페이지를 나타내는 객체.

Pintos에서는 물리 메모리를 커널 가상 주소로 접근하므로
frame->kva는 "이 frame을 커널이 접근할 때 쓰는 주소"이다.

page와 frame의 관계:
  page  = 가상 주소 쪽 장부
  frame = 실제 메모리 쪽 장부

정상적으로 메모리에 올라온 page:
  page->frame == frame
  frame->page == page
```

```text
[SPT: Supplemental Page Table]
프로세스별 추가 page table.

하드웨어 page table(PML4)은 현재 매핑된 VA -> PA만 안다.
SPT는 page fault를 복구하기 위한 더 풍부한 정보를 안다.

SPT가 답하는 질문:
  - 이 VA는 원래 존재해야 하는 page인가?
  - 존재한다면 데이터는 어디서 가져와야 하는가?
  - file인가, swap인가, zero page인가?
  - writable인가?
```

```text
[PML4 / page table]
CPU와 MMU가 직접 참조하는 하드웨어 형식의 page table.

PML4가 답하는 질문:
  - 지금 당장 이 VA를 어떤 PA/frame으로 변환할 수 있는가?
  - present bit가 켜져 있는가?
  - writable bit가 켜져 있는가?

중요:
  SPT에 page가 있어도 PML4에 present mapping이 없으면 page fault가 난다.
  이것이 lazy loading과 eviction 복구의 핵심이다.
```

### VM 타입의 의미

`struct page`는 타입별로 다르게 동작한다. Pintos는 이를
`page->operations` 함수 테이블로 처리한다.

```text
[VM_UNINIT]
뜻:
  아직 실제 타입으로 초기화되지 않은 lazy page.

언제 생기나:
  - 실행 파일 segment를 load할 때
  - mmap으로 file-backed page를 등록할 때
  - "나중에 접근하면 그때 읽자"는 lazy loading 대상일 때

상태:
  - SPT에는 등록되어 있다.
  - 하지만 아직 frame에 데이터가 없다.
  - page->uninit 안에 initializer, aux, 최종 타입 정보가 들어 있다.

첫 접근 시:
  page fault 발생
  -> swap_in(page, kva)
  -> uninit_initialize(page, kva)
  -> 최종 타입(VM_ANON 또는 VM_FILE)으로 operations 교체
  -> init(page, aux)로 실제 데이터 로드

한 줄 요약:
  "아직 안 읽었지만, fault가 나면 어떻게 만들지 알고 있는 page"
```

```text
[VM_ANON]
뜻:
  anonymous page. 특정 file 원본에 직접 묶이지 않은 메모리 page.

대표 예:
  - stack page
  - heap page
  - zero-filled page
  - file에서 다시 읽을 수 없는 일반 메모리

eviction 시:
  원본 file이 없으므로 그냥 버리면 데이터가 사라진다.
  -> swap disk에 써야 한다.

다시 접근 시:
  page fault
  -> swap slot에서 frame으로 읽어온다.

한 줄 요약:
  "파일 원본이 없어서 쫓겨나면 swap에 저장해야 하는 page"
```

```text
[VM_FILE]
뜻:
  file-backed page. 내용의 기준점이 file에 있는 page.

대표 예:
  - mmap으로 매핑한 파일 page
  - 실행 파일 segment에서 읽어온 page

eviction 시:
  clean이면 file에서 다시 읽을 수 있으므로 버릴 수 있다.
  dirty이면 변경된 내용이 있으므로 file에 write-back해야 한다.

다시 접근 시:
  page fault
  -> file offset에서 read_bytes만큼 다시 읽고
  -> page 나머지는 zero fill한다.

한 줄 요약:
  "필요하면 file에서 다시 읽을 수 있고, 변경됐으면 file에 되돌려야 하는 page"
```

```text
[VM_MARKER_0 등 marker bit]
Pintos의 enum vm_type은 하위 bit로 기본 타입을 구분하고,
상위 bit 일부를 marker로 추가 정보 표현에 쓸 수 있다.

예:
  VM_TYPE(type)으로 marker를 제거한 기본 타입만 뽑는다.

주의:
  page_get_type(page)는 VM_UNINIT일 때 page->uninit.type을 확인해
  "lazy page가 최종적으로 어떤 타입이 될지"까지 고려한다.
```

---

## 1. 예외 발생 및 fault 정보 수집

```text
[사용자 프로그램 실행 중]
CPU 명령이 어떤 가상 주소 VA에 접근
  - 예: mov, push, call, read/write 버퍼 접근, mmap 영역 접근
  - 접근 종류는 read 또는 write일 수 있다.
  ↓
MMU가 현재 프로세스의 PML4/page table을 따라가며 VA -> PA 변환 시도
  ↓
PTE 확인 결과 문제가 있음
  ├─ present = 0
  │    - page table에 물리 frame 매핑이 없다.
  │    - 아직 lazy load되지 않은 정상 page일 수도 있다.
  │    - swap out되어 잠시 메모리 밖에 있는 page일 수도 있다.
  │    - stack growth 대상일 수도 있다.
  │    - 완전히 잘못된 주소일 수도 있다.
  │
  └─ present = 1 이지만 권한 위반
       - read-only page에 write 시도
       - 유저가 접근하면 안 되는 영역 접근
       - COW를 구현했다면 write-protected 공유 page일 수도 있다.
  ↓
CPU가 page fault 예외(#PF, interrupt 14)를 발생
```

```text
[userprog/exception.c: page_fault()]
fault_addr = rcr2()
  - CR2 레지스터에는 fault를 일으킨 가상 주소가 들어 있다.
  - f->rip은 "문제가 난 명령어 주소"이고,
    CR2는 "그 명령어가 접근하려던 메모리 주소"이다.
  ↓
error_code 해석
  - not_present = (PF_P == 0)
      true  : page가 present하지 않음
      false : page는 present하지만 권한 위반
  - write = (PF_W != 0)
      true  : write 접근 중 fault
      false : read 접근 중 fault
  - user = (PF_U != 0)
      true  : user mode에서 fault
      false : kernel mode에서 fault
  ↓
vm_try_handle_fault(f, fault_addr, user, write, not_present) 호출
```

`page_fault()`는 fault를 직접 복구하지 않는다. CR2와 error code를 읽어
VM 계층으로 넘기고, VM 계층이 복구 가능하다고 판단하면 즉시 return한다.
VM 계층이 false를 반환하면 기존 project 2처럼 프로세스를 종료하거나
커널 fault 처리로 넘어간다.

---

## 2. vm_try_handle_fault(): 복구 가능한 fault인지 판정

```text
[vm/vm.c: vm_try_handle_fault()]
입력:
  - f           : interrupt frame
  - addr        : fault_addr, 즉 CR2 값
  - user        : user mode fault 여부
  - write       : write 접근 여부
  - not_present : present bit가 0이었는지 여부

  ↓
1차 검증
  ├─ addr == NULL ?
  │    -> true: 복구 불가, false 반환
  │
  ├─ user fault인데 addr가 user virtual address가 아닌가?
  │    -> true: 커널 영역 침범, false 반환
  │
  └─ not_present == false ?
       -> present page에 대한 권한 위반이다.
       -> 일반 VM만 구현 중이면 대부분 복구 불가.
       -> COW 구현 시 write fault라면 vm_handle_wp(page)에서 처리 가능.

  ↓
not_present == true인 경우
  - 아직 물리 frame이 붙지 않은 page일 수 있다.
  - SPT에 등록된 lazy page인지 확인해야 한다.
  - SPT에 없다면 stack growth 가능성을 확인해야 한다.
```

주의할 점:

- page fault가 발생했다고 해서 항상 버그는 아니다.
- project 3부터는 "정상 실행을 위한 lazy loading 신호"로 page fault를
적극적으로 활용한다.
- 반대로 SPT에도 없고 stack growth도 아닌 주소는 여전히 invalid access이다.

---

## 3. fault 주소 정규화와 SPT 조회

```text
fault_addr = addr
upage = pg_round_down(fault_addr)

예:
  fault_addr = 0x8048123
  upage      = 0x8048000

이유:
  - SPT는 byte 주소가 아니라 page 단위로 관리한다.
  - 같은 page 안의 0x8048000, 0x8048123, 0x8048fff는
    모두 동일한 struct page entry를 가리켜야 한다.
```

```text
[SPT 조회]
page = spt_find_page(&thread_current()->spt, upage)

  ├─ page != NULL
  │    -> 이 주소는 프로세스가 접근해도 되는 가상 page이다.
  │    -> 다만 아직 physical frame이 없거나,
  │       evict되어 page table 매핑이 빠진 상태이다.
  │    -> vm_claim_page(upage) / vm_do_claim_page(page)로 진행
  │
  └─ page == NULL
       -> SPT 장부에 없는 주소이다.
       -> 즉시 실패시키기 전에 stack growth 가능성을 검사한다.
```

SPT는 "이 가상 page에는 원래 어떤 데이터가 있어야 하는가?"를 기억하는
소프트웨어 장부이다.

```text
SPT entry가 들고 있어야 하는 대표 정보
  - page->va
      가상 page 시작 주소

  - page->writable
      write 가능한 page인지 여부

  - page->operations
      VM_UNINIT / VM_ANON / VM_FILE 타입별 함수 테이블
      swap_in, swap_out, destroy가 여기서 결정된다.

  - page->frame
      현재 메모리에 올라와 있다면 연결된 frame
      evict되면 NULL이 될 수 있다.

  - page->uninit
      lazy load 전용 정보
      segment/file offset/read_bytes/zero_bytes 같은 aux를 통해
      최초 접근 시 실제 내용을 채운다.

  - page->anon
      anonymous page의 swap slot 정보

  - page->file
      mmap/file-backed page의 file, offset, read_bytes 등
```

### SPT 관련 함수 역할

```text
[supplemental_page_table_init(spt)]
역할:
  새 프로세스의 SPT 자료구조를 초기화한다.

언제 호출되나:
  - 프로세스가 처음 시작될 때
  - fork로 자식 프로세스 SPT를 만들기 전에

결과:
  spt_find_page(), spt_insert_page()를 사용할 수 있는 빈 장부가 생긴다.
```

```text
[spt_find_page(spt, va)]
역할:
  주어진 va가 속한 page를 SPT에서 찾는다.

핵심:
  va를 pg_round_down(va)로 page 시작 주소에 맞춘 뒤 hash에서 찾는다.

반환:
  - 찾으면 struct page *
  - 없으면 NULL

의미:
  NULL이면 "이 주소가 무효"일 수도 있고,
  "stack growth로 새로 만들 수 있는 주소"일 수도 있다.
```

```text
[spt_insert_page(spt, page)]
역할:
  새 struct page를 SPT에 등록한다.

언제 호출되나:
  - load_segment가 lazy page를 만들 때
  - mmap이 file-backed page들을 등록할 때
  - stack growth가 새 stack page를 만들 때

실패하는 경우:
  - page->va가 page-aligned가 아님
  - 같은 va의 page가 이미 SPT에 있음
```

```text
[spt_remove_page(spt, page)]
역할:
  SPT에서 page를 제거하고 page destroy까지 수행한다.

언제 호출되나:
  - munmap으로 특정 mapping을 해제할 때
  - process exit에서 모든 page를 정리할 때

주의:
  단순히 hash에서 빼는 것만으로 끝나면 안 된다.
  frame, swap slot, file reopen, aux 같은 부속 자원도 같이 정리해야 한다.
```

---

## 4. SPT hit: 등록된 page를 claim하는 흐름

```text
[vm_claim_page(va)]
upage = pg_round_down(va)
  ↓
page = spt_find_page(&curr->spt, upage)
  ├─ NULL
  │    -> SPT에 없는 page이므로 false
  │
  └─ found
       ↓
     pml4_get_page(curr->pml4, upage) != NULL ?
       ├─ true
       │    -> 이미 매핑되어 있으므로 성공 처리
       │
       └─ false
            -> vm_do_claim_page(page)
```

`claim`은 "가상 page에 실제 물리 frame을 붙인다"는 뜻이다.

```text
[vm_do_claim_page(page)]
1. vm_get_frame()으로 frame 확보
   ↓
2. page <-> frame 양방향 연결
   - frame->page = page
   - page->frame = frame
   ↓
3. swap_in(page, frame->kva)
   - page 타입별로 frame 안에 실제 데이터를 채운다.
   - VM_UNINIT이면 이 순간 실제 타입으로 초기화된다.
   ↓
4. pml4_set_page(curr->pml4, page->va, frame->kva, page->writable)
   - 하드웨어 page table에 VA -> PA 매핑을 추가한다.
   - 이후 MMU가 같은 VA를 정상 변환할 수 있다.
   ↓
5. 성공하면 true 반환
```

### claim 관련 함수 역할

```text
[vm_claim_page(va)]
역할:
  va에 해당하는 page를 SPT에서 찾아 실제 frame을 붙이는 public 진입점.

하는 일:
  1. va를 page 단위로 정렬한다.
  2. SPT에서 struct page를 찾는다.
  3. 이미 PML4에 매핑되어 있으면 성공으로 처리한다.
  4. 매핑이 없으면 vm_do_claim_page(page)를 호출한다.

의미:
  "이 가상 주소를 지금부터 실제로 접근 가능하게 만들어라"
```

```text
[vm_do_claim_page(page)]
역할:
  claim의 실제 작업자.

하는 일:
  1. frame을 하나 확보한다.
  2. page와 frame을 서로 연결한다.
  3. page 타입에 맞게 데이터를 frame에 채운다.
  4. PML4에 page->va -> frame->kva mapping을 건다.

중요한 상태 변화:
  전:
    page->frame == NULL
    PML4[page->va] present == false

  후:
    page->frame == frame
    frame->page == page
    PML4[page->va] present == true
```

실패 시 rollback이 중요하다.

```text
swap_in 실패 또는 pml4_set_page 실패
  -> palloc_free_page(frame->kva)
  -> page->frame = NULL
  -> frame->page = NULL
  -> frame_table_remove(frame)
  -> free(frame)
  -> false 반환
```

이 rollback을 빼먹으면 다음 문제가 생긴다.

- SPT는 page가 메모리에 있다고 착각한다.
- frame table에는 사용할 수 없는 frame이 남는다.
- 같은 VA를 다시 fault 처리할 때 double mapping 또는 dangling pointer가 생긴다.

---

## 5. frame 확보와 eviction

```text
[vm_get_frame()]
struct frame 할당
  ↓
palloc_get_page(PAL_USER)로 user pool에서 물리 page 확보

  ├─ 성공
  │    -> frame->kva = kva
  │    -> frame->page = NULL
  │    -> frame_table에 등록
  │    -> frame 반환
  │
  └─ 실패
       -> free frame이 없다는 뜻
       -> vm_evict_frame()으로 기존 frame 하나를 비워 재사용
```

반드시 `PAL_USER`를 사용해야 한다. 사용자 프로세스용 page를 kernel pool에서
가져오면 테스트에서 예기치 않게 kernel memory를 고갈시킬 수 있다.

```text
[vm_evict_frame()]
victim = vm_get_victim()
  ↓
victim == NULL ?
  ├─ true
  │    -> evict할 frame 없음, 실패
  │
  └─ false
       ↓
     victim->page가 존재하는가?
       ├─ no
       │    -> 이미 비어 있는 frame이므로 그대로 재사용
       │
       └─ yes
            page = victim->page
              ↓
            swap_out(page)
              - page 타입별 backing store에 내용을 저장한다.
              - ANON: swap disk slot에 기록
              - FILE: dirty이면 원본 file에 write-back, 아니면 버릴 수 있음
              ↓
            pml4_clear_page(curr->pml4, page->va)
              - page table의 present mapping 제거
              - 다음 접근 시 다시 page fault가 나도록 만든다.
              ↓
            page->frame = NULL
            victim->page = NULL
              ↓
            victim frame 반환
```

### frame / eviction 관련 함수 역할

```text
[vm_get_frame()]
역할:
  page를 올려놓을 빈 physical frame을 구한다.

하는 일:
  1. struct frame 메타데이터를 malloc한다.
  2. palloc_get_page(PAL_USER)로 실제 user frame을 요청한다.
  3. 성공하면 frame_table에 등록하고 반환한다.
  4. 실패하면 메모리가 부족한 것이므로 vm_evict_frame()을 호출한다.

의미:
  "새 page를 담을 물리 메모리 한 칸을 내놔라"
```

```text
[vm_get_victim()]
역할:
  frame table에서 쫓아낼 frame 후보를 고른다.

현재 문서 기준 정책:
  clock / second-chance 방식

판단 기준:
  - accessed bit가 1이면 최근 사용된 것이므로 기회를 한 번 더 준다.
  - accessed bit를 0으로 내리고 다음 후보로 넘어간다.
  - accessed bit가 0인 frame을 만나면 victim으로 고른다.

의미:
  "지금 메모리에서 빼도 비교적 덜 아픈 frame을 찾아라"
```

```text
[vm_evict_frame()]
역할:
  victim frame에 들어 있던 page를 안전하게 메모리 밖으로 내보내고,
  그 frame을 새 page가 쓸 수 있도록 비운다.

왜 필요한가:
  physical memory는 한정되어 있다.
  palloc_get_page(PAL_USER)가 실패하면 새 frame을 만들 수 없으므로,
  기존 frame 하나를 비워서 재사용해야 한다.

구체적으로 하는 일:
  1. vm_get_victim()으로 희생 frame을 고른다.
  2. victim->page가 있으면 그 page의 내용을 보존한다.
     - VM_ANON이면 swap disk에 기록
     - VM_FILE이면 dirty 여부에 따라 file write-back
  3. old page의 PML4 mapping을 제거한다.
     - pml4_clear_page(curr->pml4, old_page->va)
     - 그래야 old page에 다시 접근할 때 page fault가 난다.
  4. old page와 victim frame의 연결을 끊는다.
     - old_page->frame = NULL
     - victim->page = NULL
  5. 비워진 victim frame을 반환한다.

중요한 점:
  vm_evict_frame()은 frame 자체를 free하지 않는다.
  frame->kva로 가리키는 물리 메모리 한 칸을 재사용하기 위해 반환한다.

한 줄 요약:
  "메모리가 부족할 때 기존 page를 backing store로 내보내고 frame 한 칸을 비워주는 함수"
```

현재 구현 방향은 clock/second-chance 계열이다.

```text
[vm_get_victim()]
frame_table을 clock_hand로 순회
  ↓
candidate frame 확인
  ├─ frame->page == NULL
  │    -> 즉시 victim
  │
  ├─ accessed bit == 1
  │    -> 최근 사용된 page
  │    -> pml4_set_accessed(..., false)
  │    -> 한 번 기회를 주고 다음 frame으로 이동
  │
  └─ accessed bit == 0
       -> 최근 사용되지 않은 page
       -> victim 선정
```

eviction에서 반드시 맞아야 하는 상태 변화:

```text
eviction 전
  page->frame == victim
  victim->page == page
  PML4[page->va] present == true

eviction 후
  page->frame == NULL
  victim->page == NULL
  PML4[page->va] present == false
  page 데이터는 backing store에 보존됨
```

---

## 6. swap_in(): page 타입별 데이터 적재

`swap_in(page, kva)`는 `page->operations->swap_in(page, kva)`로 위임된다.
즉 page 타입에 따라 실제 동작이 달라진다.

### 타입별 operations 함수 역할

```text
[swap_in(page, kva)]
역할:
  page에 있어야 할 데이터를 frame(kva)에 채운다.

호출 시점:
  - page fault 처리 중 vm_do_claim_page()에서 frame을 확보한 직후

타입별 실제 함수:
  - VM_UNINIT -> uninit_initialize(page, kva)
  - VM_ANON   -> anon_swap_in(page, kva)
  - VM_FILE   -> file_backed_swap_in(page, kva)

의미:
  "이 page가 원래 가져야 할 내용을 지금 frame 안에 만들어라"
```

```text
[swap_out(page)]
역할:
  frame에 있는 page 내용을 backing store로 내보낸다.

호출 시점:
  - eviction에서 victim frame을 비우기 직전

타입별 실제 함수:
  - VM_ANON -> anon_swap_out(page)
  - VM_FILE -> file_backed_swap_out(page)
  - VM_UNINIT은 아직 frame에 올라온 적 없는 lazy page라 swap_out 대상이 아니다.

의미:
  "이 frame을 비워야 하니, page 내용을 잃어버리지 않게 저장해라"
```

```text
[uninit_initialize(page, kva)]
역할:
  VM_UNINIT page를 최초 접근 시 실제 타입 page로 변환한다.

하는 일:
  1. page->uninit에 저장된 최종 타입을 확인한다.
  2. page_initializer를 호출해 operations를 anon_ops 또는 file_ops로 바꾼다.
  3. init(page, aux)를 호출해 실제 내용을 채운다.

예:
  실행 파일 segment lazy page
    -> lazy_load_segment(page, aux)
    -> file에서 read_bytes 읽기
    -> zero_bytes만큼 0 채우기

의미:
  "미뤄 둔 초기화를 지금 실행해서 진짜 page로 바꿔라"
```

```text
[anon_initializer(page, type, kva)]
역할:
  page를 anonymous page로 쓸 수 있게 operations와 anon metadata를 세팅한다.

결과:
  page->operations = anon_ops

주의:
  initializer는 page의 타입별 행동을 세팅하는 함수이고,
  실제 데이터 로드는 anon_swap_in 또는 lazy init callback에서 이뤄진다.
```

```text
[file_backed_initializer(page, type, kva)]
역할:
  page를 file-backed page로 쓸 수 있게 operations와 file metadata를 세팅한다.

결과:
  page->operations = file_ops

주의:
  file, offset, read_bytes 같은 정보는 page->file 또는 aux 쪽에 있어야
  file_backed_swap_in에서 정확한 위치를 읽을 수 있다.
```

```text
[VM_UNINIT]
상태:
  - SPT에는 등록되어 있지만 아직 실제 내용을 읽지 않은 page
  - 실행 파일 segment, mmap page, stack page 등이 lazy 상태로 존재 가능

흐름:
  uninit_initialize(page, kva)
    ↓
  uninit->page_initializer(page, uninit->type, kva)
    - page->operations를 실제 타입으로 교체
    - VM_ANON이면 anon_ops
    - VM_FILE이면 file_ops
    ↓
  init(page, aux)
    - lazy_load_segment 같은 초기화 콜백 실행
    - file_read_at으로 read_bytes만큼 읽음
    - 남은 zero_bytes는 0으로 채움

결과:
  VM_UNINIT page가 실제 VM_ANON 또는 VM_FILE page로 변한다.
```

```text
[VM_ANON]
상태:
  - stack, heap, 일반 anonymous memory
  - file에서 다시 읽을 원본이 없는 page

swap_in:
  ├─ swap slot을 가진 page
  │    -> swap disk에서 PGSIZE만큼 읽어 frame에 복원
  │    -> 읽기 성공 후 swap slot 해제
  │
  └─ swap slot이 없는 새 anonymous page
       -> 보통 zero page로 초기화

swap_out:
  -> 비어 있는 swap slot 할당
  -> frame 내용을 swap disk slot에 기록
  -> page metadata에 slot 번호 저장
```

```text
[VM_FILE]
상태:
  - 실행 파일 segment 또는 mmap된 file-backed page
  - 원본 file, offset, read_bytes, zero_bytes 같은 정보가 필요

swap_in:
  -> file의 offset에서 read_bytes만큼 frame에 읽음
  -> page 끝까지 남는 영역은 0으로 채움

swap_out:
  ├─ dirty page
  │    -> mmap page라면 file에 write-back
  │    -> 실행 파일 read-only segment라면 보통 write-back 대상 아님
  │
  └─ clean page
       -> 원본 file에서 다시 읽을 수 있으므로 그냥 버릴 수 있음
```

---

## 7. SPT miss: stack growth 검사

SPT에 page가 없다고 해서 항상 즉시 종료하면 안 된다. 스택은 아래 방향으로
자라기 때문에, 현재 스택 포인터 근처의 미등록 주소 접근은 정상적인 확장일
수 있다.

```text
[SPT miss]
page == NULL
  ↓
stack growth 후보인지 검사

대표 조건:
  - addr < USER_STACK
  - addr >= USER_STACK - 최대 스택 크기(보통 1MB)
  - addr가 현재 rsp 근처
      예: addr >= rsp - 8
      push 같은 명령은 rsp보다 약간 아래 주소를 먼저 접근할 수 있다.
```

```text
stack growth 조건 만족
  ↓
vm_stack_growth(addr)
  - addr를 pg_round_down하여 새 stack page VA 계산
  - VM_ANON page를 SPT에 등록
  - 즉시 claim하여 frame을 붙임
  - frame을 0으로 초기화
  ↓
fault 처리 성공
```

```text
stack growth 조건 불만족
  ↓
invalid access
  - SPT에 없음
  - stack 확장도 아님
  - mmap 영역도 아님
  - lazy segment도 아님
  ↓
vm_try_handle_fault() false 반환
  ↓
page_fault()가 프로세스 종료 처리
```

kernel mode에서 user pointer를 접근하다가 fault가 날 수 있으므로 `rsp` 선택도
주의해야 한다.

```text
user mode fault:
  - f->rsp를 기준으로 stack growth 검사

kernel mode fault:
  - syscall 처리 중 user buffer를 접근하다 fault가 날 수 있음
  - 이때 f->rsp는 kernel stack일 수 있다.
  - thread 구조체에 저장해 둔 user rsp를 기준으로 검사해야 한다.
```

---

## 8. mmap, munmap, file-backed page와 연결되는 흐름

```text
[mmap(addr, length, writable, fd, offset)]
입력 검증
  - addr page-aligned인지
  - length > 0인지
  - fd가 유효한 file인지
  - mapping range가 기존 SPT entry와 겹치지 않는지
  - offset이 page-aligned인지
  ↓
file_reopen()
  - fd close와 mmap 수명을 분리하기 위해 별도 file 객체 확보
  ↓
range를 page 단위로 나누어 SPT 등록
  - 각 page는 VM_FILE + VM_UNINIT 상태로 시작
  - 즉시 file 내용을 읽지 않는다.
  - 최초 접근 page fault에서 읽는다.
```

```text
[mmap 영역 최초 접근]
VA 접근
  ↓
page fault
  ↓
SPT에서 VM_UNINIT/VM_FILE page 발견
  ↓
claim
  ↓
uninit_initialize
  ↓
file_backed_initializer
  ↓
lazy_load_segment 또는 file-backed load callback
  ↓
frame에 file 내용 적재
  ↓
PML4 매핑
```

```text
[munmap(addr) 또는 process exit]
mapping range 순회
  ↓
각 page 확인
  ├─ page가 메모리에 있고 dirty
  │    -> file에 write-back
  │
  ├─ page가 메모리에 있지만 clean
  │    -> write-back 없이 frame 해제 가능
  │
  ├─ page가 아직 lazy 상태
  │    -> 읽은 적이 없으므로 write-back 불필요
  │
  └─ page가 evict된 상태
       -> ANON이면 swap slot 정리
       -> FILE이면 dirty 보존 정책에 따라 file 반영 필요
  ↓
SPT entry 제거
frame / file / aux / swap slot 등 자원 해제
```

---

## 9. fork와 SPT copy 흐름

```text
[process_fork()]
부모의 SPT를 자식 SPT로 복사
  ↓
부모 SPT의 각 page 순회

  ├─ VM_UNINIT
  │    -> 아직 로드되지 않은 page
  │    -> aux와 initializer 정보를 복사해 자식도 lazy 상태로 등록
  │
  ├─ VM_ANON
  │    -> 부모 page 내용이 메모리에 있으면 자식 frame을 새로 할당해 복사
  │    -> 부모 page가 swap out 상태라면 swap metadata를 어떻게 복제할지 정책 필요
  │
  └─ VM_FILE
       -> file-backed metadata 복사
       -> mmap file이면 file reopen/reference 수명 주의
       -> COW 미구현이면 필요한 경우 별도 frame에 내용 복사
```

COW를 구현하지 않는 기본 흐름에서는 부모와 자식이 같은 writable frame을
공유하면 안 된다. 둘 중 하나가 쓰면 다른 쪽 메모리가 오염되기 때문이다.

COW를 구현하는 경우에는 흐름이 달라진다.

```text
[COW fork]
부모/자식이 같은 frame을 공유
  ↓
양쪽 PTE를 read-only로 설정
  ↓
write 접근 발생
  ↓
present page에 write fault
  ↓
vm_handle_wp(page)
  ↓
공유 frame refcount 확인
  ├─ refcount > 1
  │    -> 새 frame 할당
  │    -> 기존 frame 내용 복사
  │    -> fault를 낸 process의 PTE만 writable 새 frame으로 교체
  │
  └─ refcount == 1
       -> 더 이상 공유가 아니므로 PTE writable만 복구
```

---

## 10. 종료 시 자원 정리 흐름

```text
[process_exit()]
supplemental_page_table_kill(&curr->spt)
  ↓
SPT hash의 모든 page destroy
  ↓
page 타입별 destroy 호출

  ├─ VM_UNINIT
  │    -> lazy aux 해제
  │    -> file reopen이 있었다면 닫기
  │
  ├─ VM_ANON
  │    -> frame에 올라와 있으면 frame 해제
  │    -> swap slot을 들고 있으면 bitmap 해제
  │
  └─ VM_FILE
       -> dirty mmap page write-back
       -> frame 해제
       -> file close
```

정리 단계의 목표:

- frame table에 죽은 process의 frame이 남지 않게 한다.
- swap bitmap에 회수 가능한 slot이 계속 사용 중으로 남지 않게 한다.
- mmap dirty page가 손실되지 않게 한다.
- SPT hash entry와 `struct page` 메모리를 해제한다.

---

## 11. 전체 통합 흐름도

```text
[A. 메모리 접근]
User program instruction accesses VA
  - 사용자가 직접 malloc/stack/mmap 영역을 접근할 수도 있고,
    syscall이 user buffer를 읽고 쓰다가 접근할 수도 있다.
  - 아직 물리 frame이 붙지 않은 VA라면 여기서부터 VM 흐름이 시작된다.
  ↓
[B. MMU 변환]
PML4/PTE lookup
  ├─ present && permission OK
  │    -> 정상 접근
  │    -> CPU가 VA를 PA로 변환해 그대로 메모리 읽기/쓰기 수행
  │
  └─ not present or permission fault
       -> #PF
       -> page fault는 "무조건 에러"가 아니라 lazy load/stack growth/swap-in 신호일 수 있음
       ↓
[C. exception.c: page_fault()]
rcr2()로 fault_addr 획득
  - CR2 = 접근에 실패한 가상 주소
  - f->rip = fault를 일으킨 명령어 주소
error_code에서 not_present/write/user 해석
  - not_present: PTE present bit가 0이었는가?
  - write      : write 접근 중 fault였는가?
  - user       : user mode에서 fault였는가?
vm_try_handle_fault(f, fault_addr, user, write, not_present)
       ↓
[D. vm_try_handle_fault()]
역할:
  이 fault가 VM이 복구할 수 있는 fault인지 판단하는 중앙 진입점.

기본 검증
  ├─ NULL addr
  │    -> 명백히 잘못된 접근, false
  │
  ├─ user가 kernel vaddr 접근
  │    -> user process가 커널 주소에 접근한 것, false
  │
  └─ present page 권한 위반
       -> 일반 VM에서는 false
       -> COW 구현 시 write fault라면 vm_handle_wp(page)로 복구 가능
       ↓
복구 불가면 false 반환
복구 가능성이 있으면 SPT 조회로 진행
       ↓
[E. SPT 조회]
upage = pg_round_down(fault_addr)
page = spt_find_page(spt, upage)

spt_find_page 역할:
  SPT에서 이 VA가 합법적으로 존재해야 하는 page인지 찾는다.
  SPT는 "데이터 출처와 page 상태"를 아는 소프트웨어 장부이다.

  ├─ page found
  │    -> 이 VA는 합법적인 page이다.
  │    -> 다만 지금 PML4에 present mapping이 없어서 fault가 났다.
  │    ↓
  │  [F. Claim]
  │  vm_claim_page(upage)
  │    - 역할: SPT에서 page를 찾아 실제 frame을 붙이는 진입점
  │    - 이미 PML4에 매핑되어 있으면 성공 처리
  │    ↓
  │  vm_do_claim_page(page)
  │    - 역할: frame 확보, 데이터 적재, PML4 매핑까지 수행하는 실제 작업자
  │    ↓
  │  vm_get_frame()
  │    - 역할: page를 담을 물리 frame 한 칸 확보
  │    ├─ free frame 있음
  │    │    -> palloc_get_page(PAL_USER) 성공
  │    │    -> 새 frame metadata 생성 후 frame_table에 등록
  │    └─ free frame 없음
  │         -> physical memory 부족
  │         -> vm_evict_frame()
  │              - 역할: 기존 page 하나를 메모리 밖으로 내보내고 frame 재사용
  │              -> vm_get_victim()
  │                   - 역할: clock/second-chance 정책으로 쫓아낼 frame 선정
  │              -> swap_out(victim->page)
  │                   - 역할: victim page 내용을 backing store에 보존
  │                   - VM_ANON: swap disk slot에 기록
  │                   - VM_FILE: dirty이면 file에 write-back, clean이면 버릴 수 있음
  │              -> pml4_clear_page()
  │                   - 역할: old page의 present mapping 제거
  │                   - 이후 old page 접근 시 다시 page fault 발생
  │              -> old_page->frame = NULL
  │              -> victim->page = NULL
  │              -> victim frame 재사용
  │    ↓
  │  page <-> frame 연결
  │    - frame->page = page
  │    - page->frame = frame
  │    ↓
  │  swap_in(page, frame->kva)
  │    - 역할: page가 가져야 할 실제 데이터를 frame 안에 채움
  │    - page->operations->swap_in으로 타입별 함수 호출
  │    ├─ VM_UNINIT
  │    │    의미:
  │    │      아직 실제 내용을 읽지 않은 lazy page
  │    │    처리:
  │    │      -> uninit_initialize(page, kva)
  │    │      -> page_initializer로 VM_ANON 또는 VM_FILE operations 세팅
  │    │      -> lazy_load_segment 같은 init callback 실행
  │    │      -> file read + zero fill
  │    │    결과:
  │    │      VM_UNINIT이 실제 VM_ANON 또는 VM_FILE page로 변함
  │    │
  │    ├─ VM_ANON
  │    │    의미:
  │    │      stack/heap처럼 원본 file이 없는 anonymous page
  │    │    처리:
  │    │      -> swap slot이 있으면 swap disk에서 복구
  │    │      -> swap slot이 없으면 zero page로 초기화(처음 쓰는 page면 0으로 비운 frame을 준다.)
  │    │    eviction:
  │    │      -> file 원본이 없으므로 swap_out 때 swap disk에 저장해야 함
  │    │
  │    └─ VM_FILE
  │         의미:
  │           실행 파일 segment 또는 mmap처럼 file에 기반한 page
  │         처리:
  │           -> file offset에서 read_bytes만큼 읽음
  │           -> 나머지 영역 zero fill
  │         eviction:
  │           -> dirty이면 file write-back
  │           -> clean이면 file에서 다시 읽을 수 있으므로 버릴 수 있음
  │    ↓
  │  pml4_set_page(page->va, frame->kva, writable)
  │    - 역할: 하드웨어 page table에 VA -> frame mapping 등록
  │    - 이 순간부터 MMU가 해당 VA를 정상 변환 가능
  │    ↓
  │  true 반환
  │
  └─ page not found
       -> SPT에 없는 주소
       -> 보통 invalid access지만 stack growth 예외 가능
       ↓
     [G. Stack Growth 검사]
     addr가 USER_STACK 아래, 최대 스택 크기 안, rsp 근처인가?
       ├─ yes
       │    -> vm_stack_growth(addr)
       │         역할:
       │           스택이 아래 방향으로 자라는 정상 접근이라고 보고
       │           새 stack page를 만든다.
       │    -> addr를 pg_round_down
       │    -> 새 VM_ANON page SPT 등록
       │         이유:
       │           stack은 file 원본이 없는 anonymous memory이기 때문
       │    -> claim해서 frame 연결
       │    -> frame zero fill
       │    -> true 반환
       │
       └─ no
            -> false 반환
       ↓
[H. page_fault() 후속 처리]
vm_try_handle_fault가 true
  -> page_fault() return
  -> CPU가 fault 난 instruction 재실행
  -> 이번에는 PML4 mapping이 있으므로 정상 진행

vm_try_handle_fault가 false
  -> invalid access
  -> user fault면 exit(-1)
  -> kernel fault면 kill/panic/debug path
```

이 통합 흐름에서 각 함수의 한 줄 역할은 다음처럼 기억하면 된다.

```text
page_fault()
  CPU 예외를 받아 CR2/error_code를 해석하고 VM 핸들러로 넘긴다.

vm_try_handle_fault()
  fault가 복구 가능한지 판단하고 SPT hit 또는 stack growth로 분기한다.

spt_find_page()
  fault 주소가 프로세스의 합법적인 page인지 SPT에서 찾는다.

vm_claim_page()
  SPT에 있는 page를 실제 접근 가능한 상태로 만드는 진입점이다.

vm_do_claim_page()
  frame 확보, swap_in, PML4 매핑을 순서대로 수행한다.

vm_get_frame()
  빈 물리 frame을 구하고, 없으면 eviction을 요청한다.

vm_get_victim()
  frame table에서 내보낼 frame을 replacement policy로 고른다.

vm_evict_frame()
  victim page를 swap/file에 보존하고 PML4 매핑을 지운 뒤 frame을 비운다.

swap_in()
  page 타입에 맞게 frame 안에 데이터를 채운다.

swap_out()
  page 타입에 맞게 frame의 데이터를 backing store로 내보낸다.

pml4_set_page()
  CPU가 실제로 접근할 수 있도록 VA -> frame mapping을 page table에 등록한다.

pml4_clear_page()
  evict된 old page의 mapping을 제거해 다음 접근 때 다시 fault가 나게 한다.

vm_stack_growth()
  합법적인 스택 확장 접근이면 새 VM_ANON page를 만들고 claim한다.
```

---

## 12. 디버깅할 때 보는 체크포인트

```text
page fault가 무한 반복된다
  - pml4_set_page가 실제로 성공했는가?
  - page->va가 page-aligned인가?
  - fault_addr를 pg_round_down해서 SPT 조회하는가?
  - swap_in 후 page->operations가 올바른 타입으로 바뀌었는가?
```

```text
lazy load 테스트가 실패한다
  - load_segment에서 즉시 file을 읽고 있지 않은가?
  - VM_UNINIT page가 SPT에 제대로 등록되는가?
  - aux에 file offset/read_bytes/zero_bytes가 정확히 저장되는가?
  - lazy_load_segment가 read_bytes만 읽고 나머지를 0으로 채우는가?
```

```text
stack growth 테스트가 실패한다
  - SPT miss일 때 바로 false를 반환하지 않는가?
  - rsp - 8 같은 push 여유 범위를 고려하는가?
  - USER_STACK - 1MB 하한을 지키는가?
  - kernel mode fault에서 user rsp를 따로 참조하는가?
```

```text
eviction 이후 복구가 실패한다
  - victim 선정 후 swap_out이 성공했는가?
  - pml4_clear_page로 old mapping을 제거했는가?
  - evicted page->frame을 NULL로 만들었는가?
  - 재사용 frame->page를 새 page로 다시 연결했는가?
  - ANON page라면 swap slot 번호가 page metadata에 저장되는가?
```

```text
mmap write-back이 실패한다
  - mmap 시 file_reopen으로 mapping 수명을 fd와 분리했는가?
  - dirty bit를 pml4_is_dirty로 확인하는가?
  - partial page는 read_bytes만 file에 쓰는가?
  - munmap/process_exit/eviction 경로가 같은 write-back 규칙을 쓰는가?
```

---

## 13. 짧은 상태 전이 요약

```text
Lazy executable/file page:
  VM_UNINIT in SPT
    -> first page fault
    -> frame allocated
    -> file data loaded
    -> VM_FILE or VM_ANON
    -> PML4 present

Anonymous page:
  VM_ANON + frame present
    -> memory pressure
    -> swap_out to swap slot
    -> PML4 cleared, page->frame NULL
    -> next access page fault
    -> swap_in from slot
    -> PML4 present

File-backed mmap page:
  VM_UNINIT/VM_FILE in SPT
    -> first access loads from file
    -> write may set dirty bit
    -> eviction/munmap/exit checks dirty
    -> dirty page writes back to file
```

최종적으로 VM의 모든 흐름은 다음 불변식을 지키는지로 점검할 수 있다.

```text
1. SPT는 "이 VA가 합법인가, 데이터 출처가 어디인가"를 안다.
2. frame table은 "어떤 물리 frame을 누가 쓰는가"를 안다.
3. swap table은 "메모리 밖으로 나간 anonymous page가 어디 있는가"를 안다.
4. PML4는 "지금 당장 CPU가 접근 가능한 VA -> PA 매핑"만 가진다.
5. page fault handler는 이 네 장부를 일관되게 맞춘 뒤 명령어를 재실행시킨다.
```

---

## 14. 처음 VM을 구현하는 사람에게 설명하는 대본

아래 대본은 `## 11. 전체 통합 흐름도`를 기준으로, VM을 처음 구현하는 사람에게
옆에서 설명한다고 생각하고 작성한 것이다. 시간 제한 없이 차근차근 설명하는
버전이라, 스터디 발표나 팀원 온보딩 때 그대로 읽어도 된다.

### 14.1. 도입

```text
오늘 설명할 내용은 Pintos project 3의 VM 흐름입니다.

먼저 결론부터 말하면, project 3의 VM은 page fault를 무조건 에러로 보지 않습니다.
project 2까지는 page fault가 나면 대부분 "잘못된 주소에 접근했구나" 하고
프로세스를 죽이면 됐습니다.

그런데 project 3부터는 page fault가 정상적인 실행 흐름의 일부가 됩니다.

예를 들어 실행 파일의 어떤 page를 아직 메모리에 안 올려뒀다가,
프로그램이 그 주소에 처음 접근하는 순간 page fault가 납니다.
그러면 커널은 "아, 이제 이 page가 필요하구나" 하고 file에서 읽어서
물리 메모리에 올립니다.

또 stack이 아래로 자라다가 아직 만들지 않은 stack page에 접근해도
page fault가 납니다.
이 경우도 조건이 맞으면 invalid access가 아니라 stack growth로 처리합니다.

그리고 물리 메모리가 부족해서 어떤 page를 swap disk로 내보냈다면,
그 page에 다시 접근할 때도 page fault가 납니다.
이때는 swap disk에서 다시 읽어오면 됩니다.

그래서 오늘 흐름의 핵심은 이것입니다.

  page fault 발생
  -> 이 fault가 복구 가능한지 판단
  -> SPT에서 page 정보를 찾음
  -> frame을 확보
  -> page 타입에 맞게 데이터 적재
  -> PML4에 VA -> frame mapping 등록
  -> fault 났던 명령어를 다시 실행

이 흐름만 잡히면 VM 구현의 큰 줄기는 잡힌 겁니다.
```

### 14.2. VM에서 관리하는 네 가지 장부

```text
본격적으로 함수 흐름을 보기 전에, VM이 관리하는 장부를 네 개로 나눠서 보면
훨씬 이해하기 쉽습니다.

첫 번째는 SPT입니다.
SPT는 Supplemental Page Table입니다.
하드웨어 page table이 아니라, 우리가 소프트웨어로 관리하는 추가 장부입니다.

SPT가 아는 것은 이런 정보입니다.

  "이 가상 주소는 합법적인 주소인가?"
  "합법적이라면 이 page의 데이터는 어디서 가져와야 하는가?"
  "file에서 읽어야 하는가?"
  "swap disk에서 복구해야 하는가?"
  "그냥 zero page로 만들면 되는가?"
  "writable한 page인가?"

두 번째는 frame table입니다.
frame은 실제 물리 메모리 한 page입니다.
page가 가상 주소 쪽 개념이라면, frame은 물리 메모리 쪽 개념입니다.

SPT가 "가상 page 장부"라면 frame table은
"현재 어떤 물리 frame을 누가 쓰고 있는지"를 관리합니다.
그리고 메모리가 부족할 때 어떤 frame을 쫓아낼지도 frame table을 보고 결정합니다.

세 번째는 swap table입니다.
anonymous page가 메모리에서 쫓겨날 때, 그 내용을 저장할 곳이 swap disk입니다.
swap table은 swap disk의 어떤 slot이 사용 중인지, 어떤 slot이 비어 있는지
기억해야 합니다.

네 번째는 PML4입니다.
PML4는 CPU와 MMU가 실제로 보는 하드웨어 page table입니다.
여기에 VA -> PA mapping이 있어야 CPU가 그 주소를 진짜로 접근할 수 있습니다.

중요한 차이가 있습니다.

SPT에 page가 있다고 해서 CPU가 바로 접근할 수 있는 것은 아닙니다.
SPT는 "이 page는 합법이고, 필요하면 이렇게 만들면 된다"는 장부입니다.

반면 PML4는 "지금 당장 이 VA가 어느 frame에 연결되어 있다"는 장부입니다.

그래서 SPT에는 있는데 PML4에는 없는 상태가 가능합니다.
이 상태에서 사용자가 그 VA에 접근하면 page fault가 나고,
그때 SPT 정보를 보고 PML4 mapping을 만들어주는 것이 VM의 핵심 흐름입니다.
```

### 14.3. VM 타입 먼저 이해하기

```text
이제 page 타입을 봐야 합니다.
Pintos의 page는 크게 VM_UNINIT, VM_ANON, VM_FILE 같은 타입으로 나뉩니다.

VM_UNINIT부터 보겠습니다.

VM_UNINIT은 아직 실제 내용이 메모리에 올라오지 않은 lazy page입니다.
이 page는 SPT에는 등록되어 있습니다.
하지만 아직 frame이 없고, file에서 데이터도 읽지 않았습니다.

대신 "나중에 접근하면 어떻게 초기화할지"만 알고 있습니다.

예를 들어 실행 파일 segment를 load할 때 모든 page를 즉시 file에서 읽으면
lazy loading이 아닙니다.
project 3에서는 일단 VM_UNINIT page를 SPT에 등록해 둡니다.
그리고 실제로 그 주소에 접근해서 page fault가 나면,
그때 file에서 읽고 zero fill을 합니다.

그래서 VM_UNINIT은 이렇게 기억하면 됩니다.

  "아직 안 만들었지만, fault가 나면 어떻게 만들지 알고 있는 page"

다음은 VM_ANON입니다.

ANON은 anonymous의 줄임말입니다.
특정 file 원본에 묶여 있지 않은 일반 메모리입니다.
대표적으로 stack page, heap page가 여기에 들어갑니다.

VM_ANON의 중요한 특징은 원본 file이 없다는 겁니다.
그래서 memory pressure 때문에 frame에서 쫓겨날 때 그냥 버리면 안 됩니다.
내용이 사라지니까요.

따라서 VM_ANON page는 eviction될 때 swap disk에 써야 합니다.
그리고 나중에 다시 접근하면 swap disk에서 읽어옵니다.

즉 VM_ANON은 이렇게 기억합니다.

  "file 원본이 없어서, 쫓겨나면 swap에 저장해야 하는 page"

마지막으로 VM_FILE입니다.

VM_FILE은 file-backed page입니다.
실행 파일 segment나 mmap page처럼 file과 연결된 page입니다.

이 page는 clean 상태라면 file에서 다시 읽을 수 있습니다.
그래서 clean VM_FILE page는 eviction할 때 굳이 swap에 쓸 필요가 없습니다.
그냥 PML4 mapping을 제거하고 frame을 비워도,
나중에 다시 접근하면 file에서 다시 읽으면 됩니다.

하지만 dirty라면 이야기가 달라집니다.
프로그램이 mmap 영역에 write를 해서 내용이 바뀌었다면,
그 변경분을 file에 write-back해야 합니다.

즉 VM_FILE은 이렇게 기억합니다.

  "file에서 다시 읽을 수 있고, 바뀌었으면 file에 되돌려야 하는 page"
```

### 14.4. page fault가 발생하는 순간

```text
이제 전체 흐름도로 들어가겠습니다.

사용자 프로그램이 어떤 가상 주소 VA에 접근합니다.
CPU는 이 VA를 실제 물리 주소로 바꿔야 하니까 PML4를 봅니다.

만약 PML4에 present mapping이 있고 권한도 맞으면 그냥 접근합니다.
이 경우 VM 핸들러는 등장하지 않습니다.

그런데 PML4에 mapping이 없거나, 권한이 맞지 않으면 page fault가 납니다.

여기서 중요한 점은 page fault가 났다고 바로 실패가 아니라는 겁니다.

page fault의 원인은 여러 가지입니다.

  - lazy page라서 아직 frame이 없을 수 있습니다.
  - swap out된 page라서 PML4 mapping만 빠져 있을 수 있습니다.
  - stack growth 대상일 수 있습니다.
  - 정말 잘못된 주소일 수도 있습니다.
  - COW를 구현했다면 write-protected page에 write한 것일 수도 있습니다.

이 구분을 하는 첫 번째 함수가 exception.c의 page_fault()입니다.

page_fault()는 CR2 레지스터에서 fault_addr를 읽습니다.
CR2에는 접근에 실패한 가상 주소가 들어 있습니다.

그리고 error_code를 해석합니다.

  not_present는 present bit가 0이었는지 알려줍니다.
  write는 write 중 fault였는지 알려줍니다.
  user는 user mode에서 fault였는지 알려줍니다.

page_fault()는 이 정보를 직접 다 처리하지 않고,
vm_try_handle_fault()로 넘깁니다.

즉 page_fault()의 역할은

  "CPU 예외 정보를 VM 계층이 이해할 수 있게 정리해서 넘기는 것"

입니다.
```

### 14.5. vm_try_handle_fault(): 복구 가능한 fault인지 판단

```text
이제 vm_try_handle_fault()로 들어옵니다.

이 함수는 VM fault 처리의 중앙 입구입니다.
여기서 하는 질문은 하나입니다.

  "이 page fault를 커널이 복구해도 되는가?"

먼저 기본 검증을 합니다.

addr가 NULL이면 복구할 수 없습니다.
user process가 kernel virtual address에 접근한 경우도 복구하면 안 됩니다.
present page에 대한 권한 위반도 일반 VM에서는 대부분 실패입니다.

다만 COW를 구현한다면 present page write fault를 vm_handle_wp()에서
복구할 수 있습니다.
하지만 기본 VM 구현에서는 보통 false를 반환합니다.

여기까지 통과했다면 이제 not-present fault입니다.
즉 PML4에 현재 mapping은 없지만, SPT에 합법적인 page 정보가 있을 수 있습니다.

그래서 다음 단계는 SPT 조회입니다.
```

### 14.6. SPT 조회: 이 주소가 원래 존재해야 하는가

```text
SPT를 조회하기 전에 fault_addr를 page 단위로 내립니다.

예를 들어 fault_addr가 0x8048123이면,
실제 page 시작 주소는 0x8048000입니다.

SPT는 byte 단위가 아니라 page 단위로 관리하므로,
spt_find_page()는 pg_round_down한 주소로 찾아야 합니다.

여기서 spt_find_page(spt, upage)를 호출합니다.

결과는 두 가지입니다.

첫 번째, page를 찾았습니다.

이 말은 사용자가 접근한 주소가 합법적인 가상 page라는 뜻입니다.
단지 현재 PML4 mapping이 없어서 fault가 났을 뿐입니다.
그러면 이 page를 claim하면 됩니다.

두 번째, page를 못 찾았습니다.

이 말은 SPT 장부에 없는 주소라는 뜻입니다.
보통은 잘못된 접근입니다.
하지만 stack growth는 예외입니다.
스택은 아래 방향으로 자라기 때문에, 현재 rsp 근처의 주소라면
새 stack page를 만들어줄 수 있습니다.

따라서 SPT miss일 때는 바로 실패하지 말고,
stack growth 조건을 검사해야 합니다.
```

### 14.7. SPT hit이면 claim한다

```text
SPT에서 page를 찾았다면 vm_claim_page()로 갑니다.

claim이라는 말은 "이 가상 page에 실제 물리 frame을 붙인다"는 뜻입니다.

vm_claim_page()는 먼저 SPT에서 page를 다시 찾습니다.
그리고 이미 PML4에 매핑되어 있으면 그냥 성공으로 처리합니다.

아직 매핑되어 있지 않다면 vm_do_claim_page(page)를 호출합니다.

실제 작업은 vm_do_claim_page()에서 일어납니다.

vm_do_claim_page()의 순서는 중요합니다.

  1. vm_get_frame()으로 frame을 확보한다.
  2. page와 frame을 서로 연결한다.
  3. swap_in(page, frame->kva)로 데이터를 채운다.
  4. pml4_set_page()로 VA -> frame mapping을 건다.

이 순서가 끝나면 사용자가 접근했던 VA는 이제 PML4에 등록됩니다.
그러면 page_fault()가 return한 뒤 CPU가 같은 명령어를 다시 실행할 때,
이번에는 정상적으로 메모리에 접근할 수 있습니다.

여기서 page와 frame의 연결도 중요합니다.

  page->frame = frame
  frame->page = page

이렇게 양방향 연결을 만들어야 나중에 eviction할 때
"이 frame에는 어떤 page가 들어 있지?"를 알 수 있고,
page 입장에서도 "내가 지금 메모리에 올라와 있나?"를 알 수 있습니다.
```

### 14.8. vm_get_frame(): 물리 메모리 한 칸 확보

```text
이제 vm_get_frame()을 자세히 보겠습니다.

vm_get_frame()은 새 page를 담을 물리 frame을 구하는 함수입니다.

먼저 struct frame을 malloc합니다.
이건 frame에 대한 메타데이터입니다.

그리고 palloc_get_page(PAL_USER)를 호출합니다.
여기서 PAL_USER가 중요합니다.
사용자 page는 user pool에서 가져와야 합니다.
kernel pool을 쓰면 테스트에서 이상하게 메모리가 고갈될 수 있습니다.

palloc_get_page(PAL_USER)가 성공하면,
frame->kva에 그 주소를 저장하고 frame_table에 등록한 뒤 반환합니다.

그런데 실패할 수도 있습니다.
이 말은 user pool에 남은 frame이 없다는 뜻입니다.

그러면 선택지는 하나입니다.
이미 사용 중인 frame 중 하나를 비워서 재사용해야 합니다.

이때 호출되는 함수가 vm_evict_frame()입니다.
```

### 14.9. vm_evict_frame(): 기존 page를 내보내고 frame 재사용

```text
vm_evict_frame()은 VM 구현에서 아주 중요한 함수입니다.

역할은 이렇게 말할 수 있습니다.

  "메모리가 부족하니, 기존 page 하나를 안전하게 밖으로 내보내고
   그 frame을 새 page가 쓰게 만들자"

먼저 vm_get_victim()을 호출합니다.
vm_get_victim()은 frame table에서 어떤 frame을 쫓아낼지 고릅니다.

정책은 여러 가지가 가능하지만, 보통 clock 또는 second-chance 방식을 씁니다.

accessed bit가 1이면 최근 사용된 page라고 보고 한 번 기회를 줍니다.
accessed bit를 0으로 내리고 다음 frame으로 넘어갑니다.
accessed bit가 0인 frame을 만나면 victim으로 고릅니다.

victim을 골랐다면 이제 그 frame 안의 page를 보존해야 합니다.

victim->page가 VM_ANON이면 swap disk에 써야 합니다.
anonymous page는 원본 file이 없으니까 그냥 버리면 데이터가 사라집니다.

victim->page가 VM_FILE이면 dirty 여부를 봐야 합니다.
dirty이면 file에 write-back해야 합니다.
clean이면 file에서 다시 읽을 수 있으므로 그냥 버릴 수 있습니다.

이 작업이 swap_out(page)입니다.

swap_out이 성공하면 pml4_clear_page()를 호출해서 old page의 PML4 mapping을 지웁니다.
이게 중요합니다.

mapping을 지우지 않으면 CPU는 old page가 아직 메모리에 있는 줄 알고
잘못된 frame을 접근할 수 있습니다.

그리고 연결을 끊습니다.

  old_page->frame = NULL
  victim->page = NULL

이제 victim frame은 비었습니다.
frame 자체를 free하는 것이 아니라, 새 page가 재사용할 수 있게 반환합니다.

정리하면 vm_evict_frame()은

  victim 선정
  -> page 내용 보존
  -> PML4 mapping 제거
  -> page/frame 연결 해제
  -> frame 재사용

순서로 움직입니다.
```

### 14.10. swap_in(): page 타입별로 데이터 채우기

```text
frame을 확보했다면 그 frame 안에 실제 데이터를 채워야 합니다.
이 단계가 swap_in(page, frame->kva)입니다.

여기서 swap_in이라는 이름이 조금 헷갈릴 수 있습니다.
항상 swap disk에서 읽는다는 뜻은 아닙니다.

Pintos에서는 page->operations->swap_in()을 호출하는 공통 인터페이스입니다.
즉 page 타입에 따라 실제 동작이 달라집니다.

VM_UNINIT이면 uninit_initialize()가 호출됩니다.

VM_UNINIT은 아직 실제 타입으로 초기화되지 않은 lazy page입니다.
그래서 uninit_initialize()는 먼저 page_initializer를 호출합니다.

최종 타입이 VM_ANON이면 anon_initializer,
최종 타입이 VM_FILE이면 file_backed_initializer가 호출됩니다.

이 initializer들은 page->operations를 실제 타입의 operations로 바꿉니다.

그 다음 init(page, aux)를 호출합니다.
여기서 실행 파일 segment라면 lazy_load_segment()가 실행되고,
file에서 read_bytes만큼 읽고 나머지를 zero fill합니다.

이 과정을 거치면 VM_UNINIT page는 더 이상 그냥 lazy placeholder가 아닙니다.
실제 VM_ANON 또는 VM_FILE page가 됩니다.

VM_ANON이면 anon_swap_in()입니다.

이 page가 이전에 eviction되어 swap slot을 가지고 있다면
swap disk에서 PGSIZE만큼 읽어옵니다.
읽어온 뒤에는 swap slot을 해제해야 합니다.

swap slot이 없는 새 anonymous page라면 보통 frame을 0으로 채웁니다.

VM_FILE이면 file_backed_swap_in()입니다.

file offset에서 read_bytes만큼 읽고,
page의 나머지 부분은 0으로 채웁니다.

이렇게 swap_in이 끝나면 frame 안에는 사용자가 기대한 데이터가 들어 있습니다.
```

### 14.11. pml4_set_page(): CPU가 접근할 수 있게 만들기

```text
데이터를 frame에 채웠다고 끝이 아닙니다.
CPU가 그 frame을 사용하려면 PML4에 mapping이 있어야 합니다.

그래서 vm_do_claim_page() 마지막에 pml4_set_page()를 호출합니다.

  pml4_set_page(curr->pml4, page->va, frame->kva, page->writable)

이 호출은 "이 가상 주소 page->va를 이 물리 frame에 연결하라"는 뜻입니다.

여기까지 성공하면 page fault handler는 true를 반환합니다.
page_fault()는 그대로 return합니다.

그러면 CPU는 아까 fault가 났던 명령어를 다시 실행합니다.

이번에는 PML4에 present mapping이 있으므로 정상적으로 VA가 PA로 변환되고,
프로그램은 마치 잠깐 멈췄다가 계속 실행되는 것처럼 보입니다.
```

### 14.12. SPT miss와 stack growth

```text
이번에는 SPT에서 page를 못 찾은 경우를 보겠습니다.

SPT에 없다는 말은 원칙적으로 이 주소가 합법적인 page로 등록되어 있지 않다는 뜻입니다.
그러면 보통 invalid access입니다.

하지만 stack은 예외입니다.

스택은 높은 주소에서 낮은 주소 방향으로 자랍니다.
프로그램이 push를 하거나 함수 호출을 하다 보면,
현재 stack pointer 바로 아래쪽 주소에 접근할 수 있습니다.

그 주소가 아직 SPT에 없더라도,
조건이 맞으면 "정상적인 stack growth"라고 보고 새 page를 만들어야 합니다.

대표 조건은 이렇습니다.

  addr < USER_STACK
  addr >= USER_STACK - 최대 stack 크기
  addr >= rsp - 8

rsp - 8을 보는 이유는 push 같은 명령이 rsp보다 약간 아래 주소를 먼저
건드릴 수 있기 때문입니다.

조건을 만족하면 vm_stack_growth(addr)를 호출합니다.

vm_stack_growth()는 addr를 page boundary로 내리고,
새 VM_ANON page를 SPT에 등록합니다.

왜 VM_ANON일까요?
stack은 file 원본이 없는 일반 메모리이기 때문입니다.
쫓겨나면 swap에 저장해야 하고, 다시 접근하면 swap에서 복구해야 합니다.

새 stack page를 등록한 뒤에는 claim해서 frame을 붙이고,
0으로 초기화합니다.

조건을 만족하지 않으면 진짜 invalid access이므로 false를 반환하고,
page_fault() 쪽에서 프로세스를 종료합니다.
```

### 14.13. 이 문서 기준 구현 진행 흐름

```text
이제 이 문서 묶음이 왜 이런 순서로 되어 있는지 설명하겠습니다.

test-notes의 순서는 단순히 폴더 이름을 나열한 것이 아닙니다.
앞 단계에서 만든 "상태 장부"와 "함수 연결"을 다음 단계가 그대로 사용하도록
쌓아 올리는 순서입니다.

가장 먼저 Introduction과 보조 문서를 봅니다.

여기서는 코드를 많이 고치는 것이 목적이 아닙니다.
앞으로 계속 나올 단어를 같은 의미로 쓰기 위해 준비하는 단계입니다.

  page는 가상 주소 공간의 한 칸입니다.
  frame은 실제 물리 메모리 한 칸입니다.
  SPT는 "이 가상 page가 어떤 page인지"를 기억합니다.
  PML4는 "지금 이 page가 실제 frame에 연결되어 있는지"를 기억합니다.

그리고 Virtual Addresses에서는 주소를 page 단위로 내리는 법을 봅니다.
Hash Table에서는 SPT를 hash로 만들 때 key를 어떻게 잡을지 봅니다.
Page Table에서는 pml4_set_page(), pml4_clear_page(), dirty/accessed bit를 봅니다.

이 세 개를 먼저 훑는 이유는 간단합니다.
뒤에서 나오는 거의 모든 버그가
"주소를 align하지 않았다",
"SPT key가 흔들렸다",
"SPT와 PML4 상태를 헷갈렸다"
중 하나로 시작하기 때문입니다.
```

```text
첫 번째 실제 구현 축은 Memory Management입니다.

여기서는 아직 mmap이나 swap을 완성하려고 하지 않습니다.
목표는 더 기본적입니다.

  "가상 주소 하나를 SPT에서 찾고,
   그 page에 frame을 붙이고,
   PML4에 매핑해서
   page fault를 복구할 수 있는가?"

이 질문에 답할 수 있게 만드는 단계입니다.

그래서 먼저 struct page의 모양을 잡습니다.
page->va는 반드시 page-aligned 주소여야 하고,
SPT hash의 key도 이 va 하나로 고정됩니다.

그다음 spt_insert_page(), spt_find_page(), spt_remove_page()를 만듭니다.
이 시점의 핵심은 "찾을 수 있는 장부"를 만드는 겁니다.

예를 들어 fault_addr가 0x8048123이라면,
SPT에서 0x8048123을 찾으면 안 됩니다.
pg_round_down해서 0x8048000 page를 찾아야 합니다.

그다음 copy/destroy를 붙입니다.
왜 여기서 벌써 fork/exit 이야기가 나오냐면,
SPT는 프로세스 생명주기와 같이 움직이기 때문입니다.
프로세스가 죽었는데 SPT page가 남으면 누수가 나고,
fork했는데 SPT 복사가 틀리면 자식 프로세스의 주소 공간이 깨집니다.

그 다음에 frame table로 넘어갑니다.
SPT가 "이 가상 page가 존재해야 한다"는 장부라면,
frame table은 "실제 메모리 한 칸을 누가 쓰고 있다"는 장부입니다.

여기서 vm_get_frame(), vm_claim_page(), vm_do_claim_page()가 연결됩니다.

흐름은 이렇게 됩니다.

  page fault 발생
  -> SPT에서 page 찾기
  -> vm_claim_page()
  -> vm_do_claim_page()
  -> vm_get_frame()
  -> page와 frame 연결
  -> swap_in()으로 내용 채우기
  -> pml4_set_page()로 CPU가 접근 가능하게 만들기

여기까지 되면 "SPT에 등록된 page를 실제 메모리에 올리는 기본 루트"가 생깁니다.

마지막으로 eviction과 accessed bit를 봅니다.
처음에는 free frame이 있어서 vm_get_frame()이 성공하지만,
언젠가는 물리 메모리가 부족해집니다.
그때 vm_get_victim()이 쫓아낼 frame을 고르고,
vm_evict_frame()이 그 page를 안전하게 내보낸 뒤 frame을 재사용합니다.

이 단계에서 중요한 것은 swap을 완벽히 구현하는 것이 아니라,
eviction이라는 사건이 발생했을 때 반드시 해야 하는 공통 처리를 잡는 것입니다.

  victim 선정
  -> page type별 swap_out/write_back 호출
  -> pml4_clear_page()
  -> page->frame = NULL
  -> frame->page = NULL
  -> frame 재사용

즉 Memory Management 단계는 VM의 뼈대입니다.
SPT, frame, claim, eviction이라는 뼈대가 없으면 뒤 단계는 얹을 곳이 없습니다.
```

```text
두 번째 축은 Anonymous Page입니다.

이 이름 때문에 "익명 page만 구현하는 단계인가?"라고 생각할 수 있는데,
실제로는 lazy loading의 핵심이 여기서 살아납니다.

Memory Management 단계에서 우리는 claim 루트를 만들었습니다.
하지만 claim할 page가 처음부터 실제 데이터를 다 들고 있지는 않습니다.

실행 파일 segment를 load할 때 모든 내용을 즉시 읽으면 lazy loading이 아닙니다.
그래서 이 단계에서는 일단 page를 VM_UNINIT 상태로 SPT에 등록합니다.

VM_UNINIT은 이런 뜻입니다.

  "아직 실제 내용을 읽지 않았지만,
   fault가 나면 어떻게 초기화할지 알고 있는 page"

그래서 vm_alloc_page_with_initializer()와 uninit_new()가 중요합니다.
이 함수들은 page 안에 initializer와 aux를 넣어 둡니다.

나중에 사용자가 그 주소에 접근하면 page fault가 납니다.
Memory Management에서 만든 claim 루트가 실행되고,
vm_do_claim_page() 안에서 swap_in(page, frame->kva)가 호출됩니다.

그런데 page 타입이 VM_UNINIT이면 실제로는 uninit_initialize()가 호출됩니다.
여기서 page_initializer가 실행되어 page가 VM_ANON 또는 VM_FILE 같은 실제 타입으로 바뀌고,
lazy_load_segment()가 aux에 저장된 file offset/read_bytes/zero_bytes를 사용해
frame 안에 내용을 채웁니다.

즉 Anonymous Page 단계는 앞 단계의 claim 루트에
"처음 접근할 때 데이터를 만드는 능력"을 붙이는 단계입니다.

이 단계가 끝나면 실행 파일 page를 미리 다 읽지 않아도 됩니다.
SPT에는 VM_UNINIT page만 등록해 두고,
첫 page fault 때 실제 file read와 zero fill을 수행할 수 있습니다.

그리고 anonymous page operation도 여기서 연결합니다.
stack이나 heap처럼 file 원본이 없는 page는 VM_ANON으로 다뤄야 하므로,
anon_initializer(), anon_swap_in(), anon_swap_out()의 기본 형태를 잡습니다.

다만 실제 swap disk와 slot bitmap을 완성하는 것은 뒤의 Swap In&Out 단계입니다.
여기서는 "anonymous page는 file-backed page와 다른 정책을 가진다"는 경계를 세우는 데 가깝습니다.
```

```text
세 번째 축은 Stack Growth입니다.

앞 단계까지는 SPT에 page가 있는 경우를 주로 처리했습니다.

  fault 발생
  -> SPT lookup
  -> page found
  -> claim

그런데 stack growth는 다릅니다.
처음에는 SPT에 page가 없습니다.
스택은 아래 방향으로 자라기 때문에,
현재 rsp 근처의 미등록 주소 접근은 새 stack page를 만들어야 하는 정상 상황일 수 있습니다.

그래서 이 단계에서는 vm_try_handle_fault()의 분기가 더 정교해집니다.

먼저 SPT에서 page를 찾아봅니다.
찾으면 기존처럼 claim하면 됩니다.

못 찾았다고 바로 invalid access로 죽이면 안 됩니다.
이제는 이렇게 묻습니다.

  이 주소가 user stack 범위 안인가?
  USER_STACK보다 아래인가?
  최대 stack 크기 하한보다 위인가?
  현재 rsp 근처인가?

이 조건을 만족하면 vm_stack_growth()를 호출합니다.

vm_stack_growth()는 새 page를 만듭니다.
이 page는 VM_ANON입니다.
왜냐하면 stack은 file에서 다시 읽을 수 있는 page가 아니라,
프로세스가 실행 중에 만든 일반 메모리이기 때문입니다.

그리고 새 stack page도 결국 앞에서 만든 루트를 그대로 탑니다.

  새 VM_ANON page를 SPT에 등록
  -> claim
  -> frame 확보
  -> zero fill
  -> pml4_set_page()

즉 Stack Growth 단계는 새로운 메모리 종류를 완전히 따로 만드는 것이 아닙니다.
"SPT miss 중 일부를 합법적인 신규 page 생성으로 인정하는 규칙"을 추가하는 단계입니다.
```

```text
네 번째 축은 Memory Mapped Files입니다.

여기서부터 file-backed page가 본격적으로 등장합니다.

mmap은 파일을 통째로 즉시 읽는 기능이 아닙니다.
파일의 특정 범위를 가상 주소 범위에 연결해 두고,
실제로 접근한 page만 fault 시점에 읽는 기능입니다.

그래서 mmap 구현의 첫 단계는 검증입니다.

  addr가 page-aligned인가?
  user address인가?
  length가 0은 아닌가?
  fd가 유효한가?
  offset이 page-aligned인가?
  기존 SPT entry와 겹치지 않는가?

검증이 끝나면 file-backed lazy page들을 SPT에 등록합니다.
여기서도 즉시 file 내용을 읽지 않습니다.
Anonymous Page 단계에서 만든 VM_UNINIT/initializer 구조를 다시 사용합니다.

차이는 최종 타입이 VM_FILE이라는 점입니다.

사용자가 mmap 영역에 처음 접근하면 page fault가 납니다.
SPT에서 page를 찾고, claim 루트를 타고, swap_in이 호출됩니다.
file-backed page라면 file_backed_swap_in()이 file offset에서 read_bytes만큼 읽고,
page의 남은 부분은 zero fill합니다.

그 다음 중요한 것은 write-back입니다.
mmap 영역에 사용자가 write하면 file 내용이 바뀌어야 합니다.
하지만 매번 즉시 file에 쓰는 것이 아니라,
munmap이나 process exit, 또는 eviction 시점에 dirty page만 file에 반영합니다.

그래서 이 단계의 후반부는 munmap과 file_backed_swap_out(),
file_backed_destroy()를 연결하는 흐름입니다.

정리하면 Memory Mapped Files 단계는
앞에서 만든 lazy page 구조와 claim 루트 위에
"file을 backing store로 쓰는 page"를 얹는 단계입니다.
```

```text
다섯 번째 축은 Swap In&Out입니다.

순서상 mmap 다음에 오는 것이 조금 이상해 보일 수 있습니다.
왜냐하면 eviction 이야기는 이미 Memory Management에서 나왔기 때문입니다.

여기서 구분해야 합니다.

Memory Management의 eviction은 "frame을 비우는 공통 절차"를 만드는 단계입니다.
하지만 anonymous page를 실제 swap disk에 저장하고,
다시 읽어오는 구체적인 저장소 구현은 Swap In&Out 단계에서 완성합니다.

VM_ANON page는 file 원본이 없습니다.
따라서 eviction될 때 그냥 버리면 데이터가 사라집니다.

그래서 vm_evict_frame()이 swap_out(page)를 호출했을 때,
page 타입이 VM_ANON이면 anon_swap_out()으로 들어가야 합니다.

anon_swap_out()은 swap disk의 빈 slot을 찾고,
frame 내용을 그 slot에 기록하고,
page metadata에 slot 번호를 저장합니다.

나중에 이 page에 다시 접근하면 page fault가 납니다.
SPT에는 page가 남아 있으므로 claim 루트로 들어갑니다.
swap_in(page, frame->kva)가 호출되고,
VM_ANON이면 anon_swap_in()이 저장해 둔 swap slot에서 데이터를 읽어옵니다.
읽기에 성공하면 그 slot은 다시 free로 돌려야 합니다.

즉 Swap In&Out 단계는
Memory Management에서 만들어 둔 eviction/claim 루트를
진짜 디스크 기반 복구까지 완성하는 단계입니다.
```

```text
마지막은 Copy-on-write Extra입니다.

이건 기본 VM이 안정화된 뒤에 진행합니다.
왜냐하면 COW는 page fault, writable bit, frame ownership, fork, SPT copy를 모두 건드립니다.

기본 흐름이 흔들리는 상태에서 COW를 얹으면
버그가 COW 때문인지, SPT 때문인지, frame lifetime 때문인지 구분하기 어려워집니다.

COW의 아이디어는 간단합니다.

fork할 때 부모 page 내용을 자식에게 즉시 복사하지 않습니다.
대신 같은 frame을 부모와 자식이 공유하고,
양쪽 PML4 mapping을 read-only로 만듭니다.

그러다가 둘 중 하나가 write를 시도하면 write fault가 납니다.
그 fault를 vm_handle_wp() 같은 경로에서 받아,
새 frame을 만들고 기존 내용을 복사한 뒤,
write한 쪽만 private writable frame으로 바꿉니다.

하지만 이 extra는 기본 VM의 네 장부가 이미 일관적이어야 가능합니다.

  SPT가 page 상태를 정확히 알고 있어야 하고,
  PML4 writable bit를 제어할 수 있어야 하고,
  frame refcount 또는 ownership을 관리할 수 있어야 하고,
  fork의 SPT copy가 기본적으로 동작해야 합니다.

그래서 COW는 맨 마지막입니다.
```

```text
전체 흐름을 다시 한 줄로 말하면 이렇습니다.

Memory Management:
  "page를 찾고 frame에 연결하는 VM의 뼈대"를 만든다.

Anonymous Page:
  "page를 미리 읽지 않고 첫 fault 때 초기화하는 lazy loading"을 붙인다.

Stack Growth:
  "SPT에 없는 주소 중 합법적인 stack 접근"을 새 VM_ANON page로 인정한다.

Memory Mapped Files:
  "file을 backing store로 쓰는 lazy page"와 dirty write-back을 붙인다.

Swap In&Out:
  "file 원본이 없는 anonymous page"를 eviction 후 swap disk에서 복구하게 만든다.

Copy-on-write:
  기본 VM이 안정화된 뒤 fork page 복사를 write fault 시점까지 미룬다.

이렇게 보면 test-notes의 순서는 목차가 아니라 의존성입니다.

먼저 장부를 만들고,
그 장부로 lazy page를 살리고,
SPT miss 중 stack을 예외 처리하고,
file-backed page를 연결하고,
마지막으로 anonymous page의 swap 복구를 완성하는 흐름입니다.
```

### 14.14. 구현하면서 계속 확인할 불변식

```text
VM 구현 중에는 계속 불변식을 확인해야 합니다.

첫 번째 불변식입니다.

  SPT에 있는 page는 page-aligned VA를 key로 가져야 합니다.

pg_round_down을 빼먹으면 같은 page 안의 주소인데도
SPT 조회가 실패할 수 있습니다.

두 번째 불변식입니다.

  메모리에 올라온 page는 page->frame과 frame->page가 서로를 가리켜야 합니다.

한쪽만 연결되어 있으면 eviction이나 destroy에서 문제가 납니다.

세 번째 불변식입니다.

  PML4 present mapping이 있으면 실제 frame이 있어야 합니다.

eviction했는데 pml4_clear_page를 안 하면,
CPU가 이미 다른 page에 재사용된 frame을 old page인 줄 알고 접근할 수 있습니다.

네 번째 불변식입니다.

  VM_ANON page가 evict되면 swap slot 위치를 page metadata에 기록해야 합니다.

그래야 다음 page fault 때 어디서 읽어올지 알 수 있습니다.

다섯 번째 불변식입니다.

  VM_FILE dirty page는 munmap, process exit, eviction 중 적절한 시점에 write-back되어야 합니다.

그렇지 않으면 mmap으로 수정한 파일 내용이 사라집니다.

여섯 번째 불변식입니다.

  실패 경로에서 rollback해야 합니다.

예를 들어 vm_do_claim_page()에서 frame을 얻은 뒤 swap_in이 실패하면,
frame을 해제하고 page->frame, frame->page 연결을 끊어야 합니다.

VM 버그는 성공 경로보다 실패 경로에서 많이 납니다.
```

### 14.15. 마무리 설명

```text
정리하겠습니다.

Pintos VM의 핵심은 page fault를 계기로 필요한 page를 실제 메모리에 올리는 것입니다.

page fault가 나면 page_fault()가 fault 정보를 읽고,
vm_try_handle_fault()가 복구 가능한지 판단합니다.

복구 가능하면 SPT에서 page를 찾습니다.
SPT hit이면 claim해서 frame을 붙입니다.
frame이 부족하면 eviction으로 기존 page를 내보냅니다.

frame을 확보하면 swap_in으로 page 타입에 맞게 데이터를 채웁니다.
VM_UNINIT은 lazy 초기화를 수행하고,
VM_ANON은 swap 또는 zero page로 복구하고,
VM_FILE은 file에서 읽거나 dirty write-back 정책을 따릅니다.

마지막으로 pml4_set_page()를 통해 CPU가 접근 가능한 mapping을 만들고,
fault가 났던 명령어를 다시 실행합니다.

SPT miss이면 stack growth 가능성을 봅니다.
조건이 맞으면 새 VM_ANON stack page를 만들고,
조건이 안 맞으면 invalid access로 종료합니다.

결국 모든 VM 구현은 네 장부의 일관성을 맞추는 일입니다.

  SPT는 합법적인 가상 page와 데이터 출처를 기억합니다.
  frame table은 실제 물리 frame 사용 현황을 기억합니다.
  swap table은 밖으로 나간 anonymous page 위치를 기억합니다.
  PML4는 CPU가 지금 당장 접근 가능한 mapping만 기억합니다.

이 네 장부가 같은 사실을 말하고 있으면 VM은 안정적으로 동작합니다.
네 장부 중 하나라도 거짓말을 하기 시작하면 page fault 무한 반복,
잘못된 데이터 접근, dirty page 손실, swap 복구 실패 같은 문제가 생깁니다.

그래서 VM을 구현할 때는 항상 이렇게 질문하면 됩니다.

  "이 VA는 SPT에 있는가?"
  "있다면 지금 frame이 있는가?"
  "frame이 없다면 어디서 데이터를 가져와야 하는가?"
  "frame을 붙인 뒤 PML4 mapping을 걸었는가?"
  "쫓겨난 page의 데이터는 잃어버리지 않았는가?"

이 질문들에 일관되게 답할 수 있으면,
Pintos VM의 큰 흐름은 제대로 구현되고 있는 것입니다.
```

