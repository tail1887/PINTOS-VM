# Page Fault/SPT/Frame 4인 분업 기준

이 문서는 Virtual Memory의 기본 축인 page fault 처리, supplemental page table, lazy loading, frame management를 4명이 병렬로 구현할 때의 책임 경계, 내부 테스트 기준, 병합 순서를 정리합니다.

핵심 원칙은 `테스트 파일 하나씩 분배`가 아니라 `상태 전이 + 기능 소유권`으로 나누는 것입니다. VM은 모든 기능이 page fault 경로에서 만나므로, 공용 page/SPT/frame 계약을 먼저 고정한 뒤 각 담당자가 그 계약을 사용합니다.

---

## 1. 공통 학습 범위

병렬 개발 전에 전원이 깊게 공부해야 하는 범위는 작게 유지합니다. 공통으로는 다음 흐름만 맞추면 됩니다.

```text
page_fault
-> vm_try_handle_fault
-> supplemental_page_table lookup 또는 stack growth
-> vm_do_claim_page
-> frame allocation/eviction
-> page type별 swap_in/load
-> pml4_set_page
```

### 모두가 알아야 할 것

- page fault가 발생하면 fault address와 access type으로 복구 가능 여부를 판단한다는 것
- SPT는 프로세스별 가상 페이지 메타데이터이고 pml4 mapping과 다르다는 것
- frame은 전역 물리 메모리 자원이고 page와 양방향 연결된다는 것
- lazy page는 fault 시점에 initializer를 실행해 실제 내용을 채운다는 것
- 권한 위반, kernel address, 잘못된 stack growth는 프로세스 종료로 이어진다는 것

### 깊게 공부할 영역

- A: page fault 분기, fault address/rsp/write/not-present 조건, stack growth 판단
- B: supplemental page table 구현, hash key, page insert/find/remove/copy/destroy
- C: lazy loading과 anonymous page initializer, executable segment load
- D: frame table, frame allocation, eviction victim 선정, pml4 mapping 해제

즉, 전원이 모든 구현 세부사항을 먼저 공부하지 않습니다. 공통 계약만 먼저 고정하고, 나머지는 담당 영역별로 학습합니다.

---

## 2. VM 공통 계약

담당자들이 서로 다른 파일을 수정하더라도 page 상태 표현이 갈라지면 안 됩니다. 아래 계약은 구현 이름보다 동작을 고정하기 위한 기준입니다.

### 계약 1: SPT key는 page-aligned user virtual address다

- 적용 대상: 모든 page lookup/insert/remove
- 규칙: fault address를 그대로 key로 쓰지 않고 항상 `pg_round_down()` 기준으로 조회한다.
- 규칙: 같은 upage에 대해 SPT entry는 하나만 존재한다.
- 이유: page boundary 접근에서 lookup 실패와 중복 insert를 막기 위함이다.

### 계약 2: page fault는 복구 가능성과 권한을 먼저 판정한다

- 적용 대상: `vm_try_handle_fault()`와 exception 경로
- 규칙: user address 범위를 벗어난 주소는 실패 처리한다.
- 규칙: write fault는 page가 writable일 때만 claim한다.
- 규칙: SPT에 없더라도 stack growth 조건을 만족하면 anonymous page를 새로 만든다.
- 이유: 정상 lazy load는 살리고, 잘못된 포인터는 커널 패닉 없이 종료하기 위함이다.

### 계약 3: page와 frame 연결은 claim/evict/cleanup에서 원자적으로 갱신한다

- 적용 대상: frame allocation, eviction, page destroy
- 규칙: claim 성공 시 `page->frame`과 `frame->page`를 모두 설정한다.
- 규칙: eviction 시 victim page의 pml4 mapping을 제거하고 양방향 연결을 끊는다.
- 규칙: process exit에서 frame과 page가 중복 해제되지 않도록 owner를 명확히 한다.
- 이유: stale mapping, double free, 잘못된 victim write-back을 막기 위함이다.

### 계약 4: lazy initializer는 page type별 데이터 로드 책임을 가진다

- 적용 대상: uninit page, anonymous page, executable file page
- 규칙: uninit page는 fault 시점에 최종 타입으로 초기화된다.
- 규칙: executable lazy load는 file, ofs, read_bytes, zero_bytes, writable 정보를 보존한다.
- 규칙: initializer 실패 시 frame/page 상태를 롤백한다.
- 이유: load 시점과 fault 시점을 분리하면서도 segment 내용이 정확히 복구되게 하기 위함이다.

### 구현 이름 예시

아래 이름은 예시입니다. 최종 이름은 팀이 코드에 맞춰 정하되, 위 계약의 동작은 유지합니다.

- `spt_find_page(struct supplemental_page_table *spt, void *va)`
- `spt_insert_page(struct supplemental_page_table *spt, struct page *page)`
- `vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux)`
- `vm_do_claim_page(struct page *page)`
- `vm_get_frame(void)`
- `vm_evict_frame(void)`

---

## 3. Page Fault/SPT/Frame 테스트 묶음 분배

| 담당 | 기능 소유권 | 주요 테스트 | 주요 수정 파일 |
| --- | --- | --- | --- |
| A | page fault 판정 + stack growth 진입 | `pt-*`, `page-*`, `stack-*` 초기 실패 분석 | `userprog/exception.c`, `vm/vm.c` |
| B | SPT hash table + page lifecycle | SPT insert/find/remove/copy/destroy 관련 회귀 | `vm/vm.c`, `include/vm/vm.h` |
| C | lazy loading + anonymous page | lazy executable load, anonymous zero page, fork copy 일부 | `vm/uninit.c`, `vm/anon.c`, `userprog/process.c` |
| D | frame table + eviction 기본 | frame allocation, victim selection, pml4 mapping cleanup | `vm/vm.c`, `include/vm/vm.h` |

분배가 애매한 테스트는 다음 기준으로 소유자를 정합니다.

- fault address 판정 또는 kill/복구 정책이면 A가 먼저 본다.
- SPT lookup/insert/destroy 문제면 B가 먼저 본다.
- page 내용이 file segment와 다르거나 zero fill이 틀리면 C가 먼저 본다.
- 메모리 부족, eviction, pml4 stale mapping 문제면 D가 먼저 본다.
- 여러 기능이 맞물린 실패는 fault 로그에서 가장 먼저 깨진 상태 전이를 기준으로 소유자를 정한다.

---

## 4. 공유 파일 수정 경계

`vm/vm.c`는 모든 담당자가 만지는 중심 파일이므로 병합 충돌을 줄이기 위해 아래 순서를 지킵니다.

1. B: SPT 자료구조와 `spt_find/insert/remove` 계약을 먼저 추가
2. A: `vm_try_handle_fault()`의 판정 틀과 stack growth 진입 조건 추가
3. C: uninit/anonymous initializer와 executable lazy load 연결
4. D: frame table, `vm_get_frame()`, eviction 기본 뼈대 추가
5. A/D: fault 재시도와 pml4 mapping cleanup 경계 통합

파일별 책임 경계는 다음과 같습니다.

- `userprog/exception.c`: page fault에서 VM으로 넘길 정보 정리, 비정상 fault 종료 정책
- `vm/vm.c`: SPT, fault handling, frame allocation, eviction 공통 흐름
- `vm/uninit.c`: lazy page initializer와 aux 수명
- `vm/anon.c`: anonymous page initializer, swap in/out 연결 전 기본 동작
- `include/vm/vm.h`: `struct page`, `struct frame`, SPT 관련 필드
- `userprog/process.c`: executable segment lazy load 등록, SPT init/copy/destroy 호출

공통 규칙:

- fault address는 항상 page align 후 SPT 조회에 사용한다.
- SPT entry 생성과 pml4 mapping 생성을 섞지 않는다.
- frame을 할당할 때는 `PAL_USER`를 사용한다.
- 실패 경로에서 page/frame/aux가 어느 쪽 소유인지 명확히 정리한다.

---

## 5. 테스트 기반 구현 프로세스

이 문서는 이미 VM 기본 계약, 테스트 소유권, 공유 파일 수정 경계를 정해 둔 상태입니다. 따라서 초반에 전원이 모든 구현 세부사항을 오래 같이 공부할 필요는 없습니다. 문서 계약을 기준으로 각자 담당 영역을 바로 파고들되, SPT와 frame 인터페이스 첫 병합은 빠르게 진행합니다.

### 기본 원칙

- 테스트 기반 개발은 각자 담당 테스트 묶음 단위로 진행한다.
- 초반 공동 학습은 page/SPT/frame 계약 확인과 인터페이스 확정에 집중한다.
- 각자 전체 구현을 끝낸 뒤 마지막에 한 번에 합치지 않는다.
- helper 시그니처, page 상태 필드, frame table 구조는 먼저 병합한다.
- 본인 테스트만 통과해도 page fault 공통 경로를 건드렸다면 인접 테스트까지 같이 확인한다.

### 초반 킥오프 범위

초반 30분~1시간은 전원이 아래 항목만 같이 맞춥니다.

- SPT key와 hash/equal 기준을 page-aligned upage로 고정한다.
- `struct page`와 `struct frame`의 필수 필드를 확정한다.
- `vm_try_handle_fault()`의 실패/성공 반환 규칙을 확정한다.
- lazy load aux의 소유권과 해제 시점을 확정한다.
- stack growth 허용 조건과 최대 크기를 확정한다.
- 브랜치/병합 순서와 각자 돌릴 테스트 묶음을 확정한다.

### 권장 진행 순서

1. 1차 병합: B의 SPT 기본 인터페이스와 process init/destroy 연결
2. 2차 병합: C의 lazy executable load와 anonymous zero page 기본 통과
3. 3차 병합: A의 page fault 판정과 stack growth 기본 통과
4. 4차 병합: D의 frame table과 eviction 기본 통과
5. 5차 병합: SPT copy/fork 관련 회귀 확인
6. 최종 병합: `vm` 기본 테스트 묶음 통과

### 병합 시 추가 확인

- B가 SPT key나 hash 구조를 바꾸면 A/C/D의 lookup 경로도 확인한다.
- A가 fault 판정을 바꾸면 C의 lazy load와 stack growth 테스트도 확인한다.
- C가 aux나 initializer를 바꾸면 process load 실패 경로와 SPT destroy도 확인한다.
- D가 eviction을 바꾸면 anonymous/file-backed page의 swap/write-back 경계도 확인한다.

---

## 6. 통합 기준

병합은 담당자 단위로 한 번에 끝내기보다, 공유 인터페이스와 기능 단위로 나누어 진행합니다. 병합 전에는 최소한 다음 순서로 회귀 테스트를 확인합니다.

1. B 병합 후: SPT insert/find/destroy, 기본 process 실행
2. C 병합 후: lazy load, anonymous page, segment zero fill
3. A 병합 후: page fault 복구/종료 경계, stack growth
4. D 병합 후: frame allocation, eviction, pml4 cleanup
5. 최종 병합 전: VM 기본 테스트 전체

최종 완료 기준은 각자 담당 테스트 통과가 아니라, 병합된 브랜치에서 `vm` 테스트가 공통 경로 기준으로 안정적으로 통과하는 것입니다.
