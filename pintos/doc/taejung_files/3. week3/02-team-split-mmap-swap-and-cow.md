# Mmap/Swap/Copy-on-write 4인 분업 기준

이 문서는 `memory_mapped_files`, `swap_in/out`, 그리고 extra인 `copy-on-write`를 4명이 병렬로 진행할 때의 책임 경계, 공통 계약, 병합 순서를 정리합니다.

핵심 원칙은 테스트 파일 단위 분배가 아니라, 구현 책임 단위를 `A/B/C/D`로 고정하는 것입니다. 각 담당자는 자기 구현 축을 끝까지 소유하고, 공유 인터페이스는 초기에 합의해 충돌을 줄입니다.

---

## 1. 공통 학습 범위

병렬 개발 전 전원이 공통으로 맞춰야 하는 범위는 아래 흐름입니다.

```text
mmap
-> 주소/길이/fd 검증
-> file-backed lazy page들을 SPT에 등록
-> page fault에서 file page load
-> munmap/process_exit에서 dirty page write-back
```

```text
frame 부족
-> victim frame 선정
-> page type별 swap_out/write_back
-> pml4 mapping 제거
-> fault 시 swap_in/file reload
```

### 모두가 알아야 할 것

- mmap은 즉시 파일 전체를 읽는 기능이 아니라 file-backed lazy page를 SPT에 등록하는 기능이라는 것
- munmap과 process exit은 dirty mmap page를 파일에 반영해야 한다는 것
- anonymous page와 file-backed page는 eviction 시 backing store가 다르다는 것
- swap slot은 anonymous page의 임시 저장소이고 slot 수명은 page 상태와 함께 움직인다는 것
- copy-on-write는 extra이며 기본 VM/mmap/swap이 안정화된 뒤 진행한다는 것

### 깊게 공부할 영역

- A: mmap/munmap syscall 검증, mapping range 관리, fd/file reopen 수명
- B: file-backed page load/write-back/cleanup
- C: anonymous swap in/out, swap table bitmap, eviction 연동
- D: copy-on-write extra, fork page sharing, write fault 복사, 통합 회귀

---

## 2. A/B/C/D 구현 소유권

| 담당 | 구현 소유권 | 핵심 책임 | 주요 수정 파일 |
| --- | --- | --- | --- |
| A | mmap/munmap syscall + mapping range | 주소 정렬/중복/길이/fd 검증, file reopen, page 등록, munmap 범위 순회 | `userprog/syscall.c`, `vm/file.c`, `include/threads/thread.h` |
| B | file-backed page | lazy file load, dirty page write-back, file page destroy, partial page zero fill | `vm/file.c`, `vm/vm.c` |
| C | swap table + anonymous page | swap slot bitmap, anon swap_out/swap_in, slot 해제, eviction 연동 | `vm/anon.c`, `vm/vm.c` |
| D | COW extra + 통합 검증 | fork 시 page sharing, write fault copy, refcount, mmap/swap/fork 회귀 | `vm/vm.c`, `userprog/process.c`, 필요 시 `vm/anon.c`, `vm/file.c` |

분배가 애매할 때는 다음 기준을 사용합니다.

- syscall 인자 검증, 주소 범위, fd/file 수명 문제는 A가 먼저 본다.
- mmap page 내용, partial page, dirty write-back 문제는 B가 먼저 본다.
- frame 부족 후 anonymous page 복구 실패는 C가 먼저 본다.
- fork 이후 page 공유/분리 문제는 D가 먼저 본다.
- 개별 기능은 맞는데 통합에서 깨지는 경우는 D가 먼저 보되, 원인 기능의 담당자가 함께 수정한다.

---

## 3. 구현 4분할 작업 단위

역할별 소유권을 실제 구현 작업으로 바로 옮길 때는 아래 4개 단위로 쪼개서 진행합니다.

### A: mmap/munmap syscall 경계 구현

- `mmap(addr, length, writable, fd, offset)`의 실패 조건을 정리한다.
- addr 정렬, NULL 주소, length 0, 이미 매핑된 range, invalid fd를 처리한다.
- 파일을 reopen해 mapping 수명과 fd close 수명을 분리한다.
- page 단위로 file-backed lazy page를 SPT에 등록한다.
- `munmap(addr)`에서 해당 mapping의 page들을 순회하고 cleanup을 호출한다.

### B: File-backed page 구현

- file page initializer와 lazy load를 구현한다.
- page별 file offset, read_bytes, zero_bytes, writable 정보를 보존한다.
- dirty mmap page는 munmap/process exit/eviction에서 파일에 write-back한다.
- clean page는 write-back 없이 정리한다.
- partial page의 남은 영역을 zero fill한다.

### C: Swap in/out 구현

- swap disk slot bitmap을 초기화하고 동기화한다.
- anonymous page eviction 시 slot을 할당하고 page 내용을 swap disk에 기록한다.
- fault로 anonymous page가 다시 claim될 때 slot에서 내용을 읽고 slot을 해제한다.
- swap slot 부족 시 실패 정책을 명확히 한다.
- page destroy 시 사용 중인 swap slot이 남지 않도록 정리한다.

### D: Copy-on-write extra 및 통합 경계 처리

- fork에서 page 내용을 즉시 복사하지 않고 공유할 수 있는 구조를 설계한다.
- write fault에서 공유 page를 실제 private frame으로 복사한다.
- refcount와 writable bit를 일관되게 관리한다.
- COW를 적용하지 않을 경우에도 기본 VM/mmap/swap 회귀를 담당한다.

---

## 4. 구현 계약

### 계약 1: mmap range는 page 단위로 SPT에 등록된다

- `addr`는 page-aligned user address여야 한다.
- mapping할 range 안에 이미 SPT entry가 있으면 실패한다.
- `length`가 파일 크기보다 길어도 file-backed page의 남은 영역은 zero fill한다.
- fd close와 mmap 수명은 분리하기 위해 필요한 경우 file reopen을 사용한다.

### 계약 2: dirty file-backed page만 write-back한다

- mmap page가 dirty이면 `munmap`, process exit, eviction에서 파일에 반영한다.
- clean page는 파일 write 없이 버릴 수 있다.
- write-back 범위는 page 전체가 아니라 실제 file-backed bytes 범위에 맞춘다.
- pml4 dirty bit와 page 상태를 확인하는 규칙을 팀에서 하나로 고정한다.

### 계약 3: anonymous page의 eviction 대상은 swap이다

- anonymous page가 evict되면 swap slot을 할당해 내용을 저장한다.
- swap in에 성공하면 해당 slot은 해제된다.
- page destroy 시 아직 swap slot을 소유하고 있으면 해제한다.
- swap slot bitmap과 page metadata는 항상 같은 상태를 가리켜야 한다.

### 계약 4: process exit은 munmap cleanup과 충돌하지 않는다

- 명시적 `munmap`과 process exit cleanup이 같은 page를 중복 write/free하지 않는다.
- SPT destroy는 page type별 destroy hook을 통해 backing store 정리를 수행한다.
- mmap mapping metadata가 있다면 page cleanup 이후 함께 정리한다.

### 계약 5: COW는 기본 VM 계약을 깨지 않는다

- COW를 적용해도 SPT lookup, pml4 mapping, frame ownership 규칙은 유지한다.
- write fault에서 writable 권한과 COW 상태를 구분한다.
- refcount가 0이 된 frame만 실제로 해제한다.
- extra를 진행하지 않는 경우 이 계약은 최종 구현 범위에서 제외한다.

---

## 5. 병합 순서

공유 파일 충돌을 줄이기 위해 아래 순서를 권장합니다.

1. A 1차 병합: mmap syscall 검증, mapping metadata, file reopen 수명 뼈대
2. B 1차 병합: file-backed lazy load와 partial zero fill
3. C 1차 병합: swap table 초기화와 anonymous swap_out/swap_in 기본
4. A 2차 병합: munmap range 순회와 cleanup 연결
5. B 2차 병합: dirty write-back, process exit 연동
6. C 2차 병합: eviction과 swap slot 해제 경계 보완
7. D 통합 병합: mmap/swap/fork 회귀, 필요 시 COW extra

---

## 6. 테스트 기반 진행

### 담당 테스트 묶음

- A: `mmap-*`의 invalid address, overlap, bad fd, close-after-mmap 계열
- B: `mmap-read`, `mmap-write`, `mmap-exit`, `mmap-clean`, partial page 계열
- C: `swap-*`, `page-merge-*`, 메모리 압박 후 anonymous 복구 계열
- D: `fork-*`, mmap/swap 통합 회귀, extra COW 테스트

### 진행 원칙

- 각 담당자는 자기 테스트 묶음을 먼저 통과시킨다.
- 공유 구조를 바꾸면 인접 담당 테스트를 반드시 함께 돌린다.
- mmap과 swap은 eviction에서 만나므로 B/C가 write-back 대상 판정을 같이 확인한다.
- 통합 검증은 D 단독 책임이 아니라 전원 공동으로 수행한다.
- 최종 완료 기준은 담당 테스트 통과가 아니라 `vm` 회귀 통과다.

---

## 7. 완료 기준

다음 조건을 모두 만족하면 완료로 본다.

1. mmap이 유효한 range에 file-backed lazy page를 정확히 등록한다.
2. munmap과 process exit에서 dirty page가 파일에 정확히 반영된다.
3. anonymous page가 eviction 후 swap을 통해 손실 없이 복구된다.
4. file-backed page와 anonymous page의 eviction 정책이 분리되어 있다.
5. frame/page/SPT/swap slot/file 객체가 중복 해제되거나 누수되지 않는다.
6. 통합 브랜치에서 mmap, swap, fork 관련 VM 회귀가 통과한다.
7. COW extra를 진행한 경우 기본 VM 테스트를 깨지 않고 fork/write fault 공유 해제가 동작한다.
