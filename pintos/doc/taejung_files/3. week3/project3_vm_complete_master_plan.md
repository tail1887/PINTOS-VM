# Pintos Project 3 VM Complete Master Plan

## 이 문서의 목적

이 문서는 Pintos Project 3 VM을 팀 단위로 학습하고 구현하기 위한 전체 지도다.

목표는 세 가지다.

```text
1. Project 3 전체 흐름을 한눈에 본다.
2. 어떤 순서로 학습하고 구현할지 정한다.
3. 각 머지 차시마다 A/B/C/D가 무엇을 맡을지 정한다.
```

SPT 기본 구현은 이미 공통 기반으로 보고, 이 문서는 그 이후의 기능을 중심으로 정리한다.

---

# 1. Project 3 전체 그림

## 1.1 한 줄 요약

```text
page 등록
-> page fault
-> SPT 조회
-> frame 확보
-> page table 매핑
-> 타입별 swap_in
-> 실행 재개
-> fork/exit/munmap 정리
```

## 1.2 가장 큰 흐름

```text
유저가 va 접근
-> page table에 매핑 없음
-> page fault 발생
-> SPT에서 page 정보 확인
-> frame 확보
-> 파일/스왑/0으로 내용 채움
-> page table에 va -> frame 매핑
-> 유저 프로그램 계속 실행
```

이 흐름이 Project 3의 핵심이다.

나머지 기능들은 전부 이 흐름을 실제로 동작하게 만드는 부품이다.

```text
SPT
- va로 page를 찾기 위한 설명서

frame table
- 물리 frame을 추적하고 eviction victim을 고르기 위한 목록

uninit page
- 아직 실제 내용이 올라오지 않은 lazy page

anonymous page
- stack/heap처럼 파일 원본이 없는 page

file-backed page
- 실행 파일 또는 mmap 파일을 원본으로 가지는 page

stack growth
- stack이 커질 때 새 page를 만드는 기능

mmap / munmap
- 파일을 유저 주소 공간에 연결하고 해제하는 기능

swap in / swap out
- 메모리가 부족할 때 page를 디스크로 내보내고 다시 가져오는 기능

SPT copy / kill
- fork와 exit에서 page 정보를 복사하고 정리하는 기능
```

---

# 2. 핵심 개념 지도

## 2.1 page

page는 유저 가상 주소 공간을 page size 단위로 나눈 한 조각이다.

Pintos에서는 보통 page 하나가 4KB다.

```text
개념상 page
- 유저 가상 주소 공간의 4KB 단위 영역

코드상 struct page
- 그 page를 관리하기 위한 커널 구조체
- va, frame, writable, operations, 타입별 정보를 가진다
```

## 2.2 frame

frame은 실제 물리 메모리의 page 크기 공간이다.

```text
개념상 frame
- 실제 물리 메모리 공간

코드상 struct frame
- 실제 frame을 추적하기 위한 커널 구조체
- frame->kva에 실제 frame에 접근할 커널 주소를 저장
- frame->page에 연결된 page를 저장
```

## 2.3 kva

`kva`는 kernel virtual address다.

커널은 물리 주소를 직접 C 포인터처럼 쓰지 않고, 커널 가상 주소인 `kva`로 물리 frame에 접근한다.

```text
palloc_get_page(PAL_USER)
-> user pool에서 물리 frame 확보
-> 커널이 접근할 수 있는 kva 반환
```

## 2.4 page table

page table은 CPU/MMU가 실제 주소 변환에 사용하는 자료구조다.

```text
page table
- CPU가 사용
- va -> frame 매핑을 저장
- pml4_set_page()로 매핑 추가
```

SPT와 page table은 다르다.

```text
SPT
- 커널이 page fault를 복구하기 위해 참고
- va -> struct page

page table
- CPU/MMU가 실제 메모리 접근 때 사용
- va -> physical frame
```

## 2.5 page fault

page fault는 CPU가 어떤 가상 주소에 접근하려 했는데 page table을 통해 정상 접근할 수 없을 때 발생한다.

대표적인 경우:

```text
1. page table에 매핑이 없음
2. read-only page에 write 시도
3. 유저 프로그램이 kernel address 접근
4. SPT에도 없는 잘못된 주소 접근
```

Project 3에서는 page fault가 항상 에러는 아니다.

lazy loading에서는 page fault가 “이제 이 page를 실제 메모리에 올릴 때가 됐다”는 신호가 된다.

---

# 3. End-to-End 구조도

## 3.1 전체 구조도

```text
Project 3 Virtual Memory

user program
└── user virtual address 접근
    └── page table 확인
        ├── 매핑 있음
        │   └── physical frame 접근
        │       └── 유저 프로그램 계속 실행
        │
        └── 매핑 없음 또는 권한 문제
            └── page fault 발생
                └── vm_try_handle_fault() - page fault가 복구 가능한지 검사하는 입구
                    ├── 잘못된 주소
                    │   └── false 반환
                    │       └── 프로세스 종료
                    │
                    ├── stack growth 가능
                    │   └── vm_stack_growth() - stack page를 새로 늘리는 함수
                    │       └── 새 anonymous page 할당
                    │           └── page claim
                    │
                    └── 일반 lazy page
                        └── spt_find_page() - fault 주소에 해당하는 page를 SPT에서 찾는 함수
                            ├── page 없음
                            │   └── false 반환
                            │       └── 프로세스 종료
                            │
                            └── page 있음
                                └── vm_do_claim_page() - page에 frame을 붙이고 실제 메모리에 올리는 함수
                                    ├── vm_get_frame() - user pool에서 frame을 확보하는 함수
                                    │   ├── 빈 frame 있음
                                    │   │   └── palloc_get_page(PAL_USER) - user pool에서 물리 frame을 가져오는 함수
                                    │   │       └── struct frame 생성
                                    │   │           └── frame_table 등록
                                    │   │
                                    │   └── 빈 frame 없음
                                    │       └── vm_evict_frame() - 기존 frame 하나를 비워 재사용하는 함수
                                    │           └── victim frame 선정
                                    │               └── swap_out() - page 타입에 맞게 내용을 backing store로 내보내는 함수
                                    │
                                    ├── page와 frame 연결
                                    │   ├── page->frame = frame
                                    │   └── frame->page = page
                                    │
                                    ├── page table 매핑
                                    │   └── pml4_set_page() - page table에 va -> frame 매핑을 추가하는 함수
                                    │       └── page->va -> frame->kva
                                    │
                                    └── swap_in() - page 타입에 맞게 내용을 frame으로 가져오는 함수
                                        ├── VM_UNINIT
                                        │   └── uninit_initialize() - uninit page를 실제 타입으로 전환하는 함수
                                        │       ├── anon_initializer() - page를 anonymous page로 초기화
                                        │       ├── file_backed_initializer() - page를 file-backed page로 초기화
                                        │       └── init(page, aux) - 저장해둔 추가 초기화 함수 실행
                                        │           └── lazy_load_segment() - 실행 파일 segment를 frame에 읽어오는 함수
                                        │
                                        ├── VM_ANON
                                        │   └── anon_swap_in() - anonymous page 내용을 frame에 복원하는 함수
                                        │       └── swap disk에서 복원
                                        │
                                        └── VM_FILE
                                            └── file_backed_swap_in() - 파일 내용을 frame에 읽어오는 함수
                                                └── backing file에서 읽기
```

## 3.2 최종 흐름 압축

```text
1. load 단계에서 page 설명서를 SPT에 등록한다.
2. 유저가 아직 매핑되지 않은 va에 접근한다.
3. page fault가 발생한다.
4. SPT에서 해당 page를 찾는다.
5. frame을 확보한다.
6. page와 frame을 연결한다.
7. page table에 va -> frame을 매핑한다.
8. page 타입에 맞게 내용을 frame에 채운다.
9. fault가 났던 명령을 다시 실행한다.
10. 이제 매핑이 있으므로 유저 프로그램이 계속 실행된다.
```

---

# 4. 학습 및 구현 목차

아래 목차는 문서 목차가 아니라, 팀이 실제로 공부하고 구현할 기능 목차다.

```text
1. 가상 메모리 전체 흐름
2. Page와 Frame 기본 구조
3. Frame Claim
4. Uninit Page와 Lazy Loading
5. Executable Segment Lazy Loading
6. 초기 Stack Page
7. Stack Growth 판별
8. Stack Growth 실행
9. Page Destroy
10. SPT Kill
11. mmap Validation
12. mmap Page Registration
13. File-backed Swap In
14. munmap과 Write-back
15. Eviction Victim Selection
16. Eviction Flow
17. Anonymous Swap Table
18. File-backed Swap Out
19. SPT Copy - Uninit Page
20. SPT Copy - Loaded Anon Page
21. SPT Copy - File-backed/mmap Page
22. Exit와 전체 Cleanup 안정화
```

진행 체크:

```text
[ ] 1. 가상 메모리 전체 흐름 - Merge 0 / 공통
[ ] 2. Page와 Frame 기본 구조 - Merge 0 / 공통
[ ] 3. Frame Claim - Merge 1 / A
[ ] 4. Uninit Page와 Lazy Loading - Merge 1 / B
[ ] 5. Executable Segment Lazy Loading - Merge 1 / C
[ ] 6. 초기 Stack Page - Merge 1 / D
[ ] 7. Stack Growth 판별 - Merge 2 / A
[ ] 8. Stack Growth 실행 - Merge 2 / B
[ ] 9. Page Destroy - Merge 2 / C
[ ] 10. SPT Kill - Merge 2 / D
[ ] 11. mmap Validation - Merge 3 / A
[ ] 12. mmap Page Registration - Merge 3 / B
[ ] 13. File-backed Swap In - Merge 3 / C
[ ] 14. munmap과 Write-back - Merge 3 / D
[ ] 15. Eviction Victim Selection - Merge 4 / A
[ ] 16. Eviction Flow - Merge 4 / B
[ ] 17. Anonymous Swap Table - Merge 4 / C
[ ] 18. File-backed Swap Out - Merge 4 / D
[ ] 19. SPT Copy - Uninit Page - Merge 5 / A
[ ] 20. SPT Copy - Loaded Anon Page - Merge 5 / B
[ ] 21. SPT Copy - File-backed/mmap Page - Merge 5 / C
[ ] 22. Exit와 전체 Cleanup 안정화 - Merge 5 / D
```

---

# 5. 추천 머지 계획

8일 기준으로는 5번의 머지를 추천한다.

```text
Merge 1: Frame Claim + Lazy Loading 기본 흐름
Merge 2: Stack Growth + Page Cleanup 기본 흐름
Merge 3: mmap / File-backed Page 기본 흐름
Merge 4: Eviction + Swap In/Out
Merge 5: Fork/Exit 정리 + 전체 안정화
```

진행률 표시:

```text
전체 머지 5개 중 0개 완료

Merge 1: [ ] Frame Claim + Lazy Loading
Merge 2: [ ] Stack Growth + Page Cleanup
Merge 3: [ ] mmap / File-backed Page
Merge 4: [ ] Eviction + Swap In/Out
Merge 5: [ ] Fork/Exit 정리 + 전체 안정화
```

---

# 6. Merge 1 - Frame Claim + Lazy Loading

## 6.1 목표

```text
page fault가 났을 때 SPT에서 찾은 page를 실제 frame에 올리고,
page table에 매핑한 뒤, lazy loading으로 내용을 채운다.
```

## 6.2 이상적인 내부 머지 순서

```text
1. B - Uninit Page와 Initializer
2. A - Frame Claim
3. C - Executable Segment Lazy Loading
4. D - 초기 Stack Page
```

이유:

```text
B가 page 타입 전환과 swap_in 연결을 먼저 잡아야 한다.
A가 frame 확보와 page table 매핑을 잡으면 C/D가 실제 page를 올릴 수 있다.
C는 실행 파일 lazy loading, D는 초기 stack이라 A/B 이후에 붙이는 것이 안정적이다.
```

## 6.3 A - Frame Claim

```text
목표
- frame을 확보하고 page와 연결한 뒤 page table에 매핑한다.

이유
- SPT에 page가 있어도 page table에 매핑이 없으면 유저 프로그램은 접근할 수 없다.

수정/추가 위치
- include/vm/vm.h
  - struct frame 필드 보강
- vm/vm.c
  - frame_table
  - vm_get_frame()
  - vm_claim_page()
  - vm_do_claim_page()

의존성
- B의 uninit/lazy 흐름이 있어야 swap_in 이후가 자연스럽게 이어진다.
- C의 lazy_load_segment가 있어야 실행 파일 내용을 실제로 채울 수 있다.
```

## 6.4 B - Uninit Page와 Initializer

```text
목표
- uninit page를 첫 page fault 때 실제 anon/file page로 전환한다.

이유
- Project 3는 page를 처음부터 메모리에 올리지 않고, 필요할 때 초기화하는 lazy loading 구조다.

수정/추가 위치
- vm/vm.c
  - vm_alloc_page_with_initializer()
- vm/uninit.c
  - uninit_initialize()
- vm/anon.c
  - anon_initializer()
- vm/file.c
  - file_backed_initializer()

의존성
- A의 vm_do_claim_page가 swap_in을 호출해야 이 흐름이 실행된다.
- C가 넘겨주는 init/aux가 있어야 file segment lazy load가 가능하다.
```

## 6.5 C - Executable Segment Lazy Loading

```text
목표
- 실행 파일 segment를 즉시 읽지 않고, page fault 때 읽도록 등록한다.

이유
- lazy loading의 핵심은 필요한 page만 실제 메모리에 올리는 것이다.

수정/추가 위치
- userprog/process.c
  - load_segment()
  - lazy_load_segment()
  - segment load용 aux 구조체

의존성
- B의 vm_alloc_page_with_initializer가 init/aux를 저장해야 한다.
- A의 frame claim과 page table 매핑이 되어야 lazy_load_segment가 kva에 내용을 채울 수 있다.
```

## 6.6 D - 초기 Stack Page

```text
목표
- 첫 stack page를 만들고 argument passing이 정상 동작하도록 연결한다.

이유
- 유저 프로그램 시작 시 stack은 즉시 필요하며, Project 2 argument passing과 직접 연결된다.

수정/추가 위치
- userprog/process.c
  - setup_stack()
  - argument_stack 관련 흐름 확인
- 필요 시 vm/vm.c
  - vm_claim_page() 사용 흐름 확인

의존성
- A의 vm_claim_page/vm_do_claim_page가 필요하다.
- B의 anon initializer가 있어야 stack page를 anonymous page로 다룰 수 있다.
```

## 6.7 완료 기준

```text
빌드 성공
기본 user program 실행 가능
lazy-anon 또는 lazy-file 흐름 진입 확인
Project 2 기본 테스트가 크게 깨지지 않는지 확인
```

---

# 7. Merge 2 - Stack Growth + Page Cleanup

## 7.1 목표

```text
stack이 커지는 접근을 page fault에서 구분하고,
프로세스 종료 시 page 자원을 안전하게 정리한다.
```

## 7.2 이상적인 내부 머지 순서

```text
1. C - Page Destroy
2. D - SPT Kill
3. B - Stack Growth 실행
4. A - Stack Growth 판별
```

이유:

```text
C/D의 cleanup 기반이 먼저 있어야 새 stack page도 종료 시 안전하게 정리된다.
B가 실제 stack page 생성 함수를 만든 뒤, A가 fault handler에서 호출하는 흐름이 자연스럽다.
A는 page fault 입구를 건드리므로 마지막에 붙이는 것이 안정적이다.
```

## 7.3 A - Stack Growth 판별

```text
목표
- page fault가 stack growth로 처리 가능한 접근인지 판단한다.

이유
- SPT에 page가 없다고 전부 잘못된 접근은 아니며, stack 확장일 수 있다.

수정/추가 위치
- vm/vm.c
  - vm_try_handle_fault()
- userprog/syscall.c 또는 userprog/exception.c 관련 흐름
  - user rsp 저장 필요 여부 확인
- threads/thread.h
  - user rsp 저장 필드가 필요하면 추가

의존성
- B의 vm_stack_growth가 있어야 판별 후 실제 확장이 가능하다.
```

## 7.4 B - Stack Growth 실행

```text
목표
- fault 주소에 해당하는 새 anonymous page를 할당하고 claim한다.

이유
- stack은 높은 주소에서 낮은 주소로 자라므로, 필요한 순간 새 page를 만들어야 한다.

수정/추가 위치
- vm/vm.c
  - vm_stack_growth()
  - VM_ANON page 할당
  - 1MB stack limit 검사

의존성
- A가 stack growth 조건을 정확히 판단해야 한다.
- Merge 1의 frame claim과 anon initializer가 필요하다.
```

## 7.5 C - Page Destroy

```text
목표
- page 타입별 destroy 함수를 구현해 page가 가진 자원을 정리한다.

이유
- free(page)만 하면 file/swap/frame 같은 부가 자원이 남을 수 있다.

수정/추가 위치
- vm/uninit.c
  - uninit_destroy()
- vm/anon.c
  - anon_destroy()
- vm/file.c
  - file_backed_destroy() 초안 확인

의존성
- D의 supplemental_page_table_kill이 destroy(page)를 호출한다.
- Merge 4 이후 swap slot 정리 로직과 연결된다.
```

## 7.6 D - SPT Kill

```text
목표
- 프로세스 종료 시 SPT 안의 모든 page를 순회하며 정리한다.

이유
- 프로세스마다 SPT를 가지므로 exit 때 page 설명서와 관련 자원을 모두 해제해야 한다.

수정/추가 위치
- vm/vm.c
  - supplemental_page_table_kill()
  - hash_destroy 또는 hash 순회

의존성
- C의 destroy 함수들이 필요하다.
- Merge 3의 mmap cleanup, Merge 4의 swap cleanup과 나중에 다시 맞춰야 한다.
```

## 7.7 완료 기준

```text
stack growth 관련 테스트 일부 통과 기대
프로세스 종료 시 page cleanup 흐름 확인
page fault가 stack인지 invalid access인지 구분 가능
```

---

# 8. Merge 3 - mmap / File-backed Page

## 8.1 목표

```text
파일을 유저 주소 공간에 매핑하고,
page fault 시 파일 내용을 frame으로 읽어온다.
```

## 8.2 이상적인 내부 머지 순서

```text
1. C - File-backed Swap In
2. A - mmap Validation
3. B - mmap Page Registration
4. D - munmap과 Write-back
```

이유:

```text
C가 file-backed page를 실제로 읽어오는 기본 동작을 먼저 준비한다.
A가 mmap 실패 조건을 확정한 뒤 B가 안전한 범위만 SPT에 등록한다.
D는 B가 만든 mmap page 범위를 해제하고 write-back해야 하므로 마지막에 붙인다.
```

## 8.3 A - mmap Validation

```text
목표
- mmap 요청이 유효한지 검사한다.

이유
- 잘못된 주소, 길이, fd, overlap을 허용하면 SPT/page table 상태가 꼬인다.

수정/추가 위치
- vm/file.c
  - do_mmap()
- userprog/syscall.c
  - mmap syscall 연결 확인

의존성
- B가 page 등록을 하기 전에 A의 검증 조건이 먼저 확정되어야 한다.
```

## 8.4 B - mmap Page Registration

```text
목표
- mmap 영역을 page 단위로 나누어 file-backed page로 SPT에 등록한다.

이유
- mmap도 lazy loading 대상이므로 실제 파일 읽기는 page fault 때 해야 한다.

수정/추가 위치
- vm/file.c
  - do_mmap()
  - mmap용 aux 구조체
  - offset/read_bytes/zero_bytes 계산

의존성
- A의 validation을 통과한 범위만 등록해야 한다.
- C의 file_backed_swap_in이 aux 정보를 사용한다.
```

## 8.5 C - File-backed Swap In

```text
목표
- file-backed page fault 시 파일에서 데이터를 읽어 frame에 채운다.

이유
- file-backed page의 원본은 파일이므로, frame 내용은 파일에서 가져와야 한다.

수정/추가 위치
- vm/file.c
  - file_backed_initializer()
  - file_backed_swap_in()
  - 파일 read와 zero fill 처리

의존성
- B가 file, offset, read_bytes, zero_bytes 정보를 저장해야 한다.
- Merge 1의 vm_do_claim_page가 frame->kva를 넘겨줘야 한다.
```

## 8.6 D - munmap과 Write-back

```text
목표
- mmap 해제 시 수정된 page를 파일에 다시 쓰고 mapping을 제거한다.

이유
- mmap된 파일은 메모리에 쓴 내용이 파일에 반영되어야 한다.

수정/추가 위치
- vm/file.c
  - do_munmap()
  - file_backed_destroy()
  - dirty bit 확인과 write-back
- userprog/syscall.c
  - munmap syscall 연결 확인

의존성
- B가 mmap page 범위를 추적할 수 있어야 한다.
- C가 file-backed page 정보를 올바르게 초기화해야 한다.
```

## 8.7 완료 기준

```text
mmap-read 계열 일부 통과 기대
mmap validation 테스트 일부 통과 기대
munmap 시 수정 내용 write-back 흐름 확인
```

---

# 9. Merge 4 - Eviction + Swap In/Out

## 9.1 목표

```text
물리 frame이 부족할 때 기존 page를 안전하게 내보내고,
비워진 frame을 재사용한다.
```

## 9.2 이상적인 내부 머지 순서

```text
1. C - Anonymous Swap Table
2. D - File-backed Swap Out
3. A - Eviction Victim Selection
4. B - Eviction Flow
```

이유:

```text
C/D가 타입별 swap_out을 먼저 준비해야 eviction이 victim page를 안전하게 내보낼 수 있다.
A가 victim을 고르는 정책을 만든 뒤, B가 실제 eviction 전체 흐름에 연결한다.
B는 vm_get_frame과 page table mapping에 영향을 주므로 마지막에 붙이는 것이 안정적이다.
```

## 9.3 A - Eviction Victim Selection

```text
목표
- frame_table을 순회하며 쫓아낼 frame을 고른다.

이유
- free frame이 없을 때 아무 frame이나 빼면 최근 사용 중인 page를 망가뜨릴 수 있다.

수정/추가 위치
- vm/vm.c
  - vm_get_victim()
  - accessed bit 확인/초기화

의존성
- B의 eviction flow가 victim을 실제로 비워야 한다.
- Merge 1의 frame_table 등록이 되어 있어야 한다.
```

## 9.4 B - Eviction Flow

```text
목표
- victim page를 swap_out하고 page table 매핑과 page-frame 연결을 정리한다.

이유
- eviction은 page 삭제가 아니라 frame에서 내려보내는 작업이다.

수정/추가 위치
- vm/vm.c
  - vm_evict_frame()
  - vm_get_frame()에서 palloc 실패 시 eviction 연결
  - pml4_clear_page()

의존성
- A가 victim frame을 골라줘야 한다.
- C/D의 타입별 swap_out이 필요하다.
```

## 9.5 C - Anonymous Swap Table

```text
목표
- anon page를 swap disk에 저장하고 다시 복원한다.

이유
- anon page는 파일 원본이 없으므로 eviction 시 내용을 보존할 임시 저장소가 필요하다.

수정/추가 위치
- include/vm/anon.h
  - swap slot 정보 필드
- vm/anon.c
  - vm_anon_init()
  - anon_swap_out()
  - anon_swap_in()
  - swap bitmap

의존성
- B가 anon page의 swap_out을 호출해야 한다.
- A/B의 eviction 흐름이 있어야 실제로 테스트된다.
```

## 9.6 D - File-backed Swap Out

```text
목표
- file-backed page가 쫓겨날 때 dirty 여부에 따라 파일에 기록한다.

이유
- file-backed page는 원본 파일이 backing store라서, 수정된 내용만 파일에 반영하면 된다.

수정/추가 위치
- vm/file.c
  - file_backed_swap_out()
  - dirty bit 확인
  - file write-back

의존성
- B의 eviction flow가 file-backed page의 swap_out을 호출해야 한다.
- Merge 3의 file-backed page 정보가 필요하다.
```

## 9.7 완료 기준

```text
swap-anon 일부 통과 기대
swap-file 일부 통과 기대
메모리 제한이 있는 테스트에서 eviction 경로 진입 확인
```

---

# 10. Merge 5 - Fork/Exit 정리 + 전체 안정화

## 10.1 목표

```text
fork와 exit에서도 SPT/page/frame/file/swap 자원이 일관되게 복사되고 정리되도록 마무리한다.
```

## 10.2 이상적인 내부 머지 순서

```text
1. A - SPT Copy: Uninit Page
2. B - SPT Copy: Loaded Anon Page
3. C - SPT Copy: File-backed/mmap Page
4. D - Exit와 전체 Cleanup 안정화
```

이유:

```text
copy는 uninit -> anon -> file-backed 순서로 확장하는 것이 이해와 디버깅이 쉽다.
D는 A/B/C가 만든 복사 정책과 Merge 2/3/4의 cleanup을 모두 맞춰야 하므로 마지막에 붙인다.
마지막 머지에서는 기능 추가보다 중복 해제와 회귀 테스트 안정화가 핵심이다.
```

## 10.3 A - SPT Copy: Uninit Page

```text
목표
- 부모의 uninit page 정보를 자식 SPT에 같은 의미로 복사한다.

이유
- fork 후 자식도 아직 접근하지 않은 lazy page를 동일하게 사용할 수 있어야 한다.

수정/추가 위치
- vm/vm.c
  - supplemental_page_table_copy()
  - uninit page copy

의존성
- B/C와 같은 copy 함수 안에서 타입별 분기가 필요하다.
- Merge 1의 vm_alloc_page_with_initializer 흐름을 재사용한다.
```

## 10.4 B - SPT Copy: Loaded Anon Page

```text
목표
- 이미 frame에 올라온 anonymous page 내용을 자식에게 복사한다.

이유
- fork 후 부모와 자식은 독립적인 주소 공간을 가져야 한다.

수정/추가 위치
- vm/vm.c
  - supplemental_page_table_copy()
  - anon page copy
  - 자식 page claim 후 frame 내용 memcpy

의존성
- A/C와 copy 함수 구조를 맞춰야 한다.
- Merge 1의 claim 흐름과 Merge 4의 anon 상태 정보가 필요하다.
```

## 10.5 C - SPT Copy: File-backed/mmap Page

```text
목표
- file-backed page와 mmap page의 backing 정보를 자식에게 복사한다.

이유
- 자식 프로세스도 파일 기반 page fault를 올바르게 처리해야 한다.

수정/추가 위치
- vm/vm.c
  - supplemental_page_table_copy()
- vm/file.c
  - mmap/file-backed page copy에 필요한 정보 확인

의존성
- Merge 3의 mmap/file-backed page 구조가 확정되어야 한다.
- A/B와 copy 함수 안에서 충돌 없이 합쳐야 한다.
```

## 10.6 D - Exit와 전체 Cleanup 안정화

```text
목표
- exit, munmap, swap cleanup이 중복 해제 없이 정리되도록 마무리한다.

이유
- Project 3 후반 실패는 기능 구현보다 cleanup 순서와 double free에서 많이 발생한다.

수정/추가 위치
- vm/vm.c
  - supplemental_page_table_kill()
- vm/file.c
  - do_munmap()
  - file_backed_destroy()
- vm/anon.c
  - anon_destroy()

의존성
- A/B/C의 copy 정책과 맞아야 한다.
- Merge 2 cleanup, Merge 3 mmap, Merge 4 swap과 모두 연결된다.
```

## 10.7 완료 기준

```text
fork 관련 VM 테스트 일부 통과 기대
mmap-exit, mmap-inherit 계열 점검
swap-fork 계열 점검
전체 vm 테스트 회귀 확인
```

---

# 11. 8일 운영안

```text
Day 1
- 전체 그림 공유
- SPT 제외한 frame/lazy loading 학습
- Merge 1 작업 시작

Day 2
- Merge 1 구현 완료
- 코드 리뷰
- 기본 lazy 흐름 확인

Day 3
- stack growth와 cleanup 학습
- Merge 2 구현

Day 4
- Merge 2 마무리
- mmap 개념 학습
- Merge 3 작업 시작

Day 5
- Merge 3 마무리
- mmap read/write/munmap 흐름 리뷰

Day 6
- eviction과 swap 학습
- Merge 4 작업 시작

Day 7
- Merge 4 마무리
- fork/exit cleanup 학습
- Merge 5 작업 시작

Day 8
- Merge 5 마무리
- 전체 테스트
- 실패 테스트별 원인 분류와 최종 보강
```

---

# 12. 머지마다 공통 리뷰 기준

각 머지마다 아래 질문에 답하고 넘어간다.

```text
1. 이 기능은 전체 흐름 중 어느 단계인가?
2. 새로 만든 구조체 필드는 어디에 저장되는 정보인가?
3. 이 함수는 page를 다루는가, frame을 다루는가, page table을 다루는가?
4. 실패하면 어떤 자원을 정리해야 하는가?
5. 이 머지로 통과 가능성이 생긴 테스트는 무엇인가?
6. 다음 머지에서 이어받아야 하는 TODO는 무엇인가?
7. 다른 팀원의 작업과 의존성이 있는 지점은 어디인가?
```

---

# 13. 머지 기준 요약

```text
Merge 0
└── 전체 개념 정리
    ├── page / frame / kva
    ├── page table / SPT
    └── page fault end-to-end 흐름

Merge 1
└── lazy page를 실제 frame에 올리는 기본 흐름
    ├── frame claim
    ├── uninit page
    ├── executable lazy loading
    └── 초기 stack page

Merge 2
└── stack growth와 page cleanup
    ├── stack growth 판별
    ├── vm_stack_growth
    ├── destroy
    └── supplemental_page_table_kill 기본

Merge 3
└── mmap과 file-backed page
    ├── mmap validation
    ├── mmap page registration
    ├── file_backed_swap_in
    └── munmap write-back

Merge 4
└── eviction과 swap
    ├── victim selection
    ├── eviction flow
    ├── anonymous swap table
    └── file-backed swap_out

Merge 5
└── fork/exit 안정화
    ├── SPT copy
    ├── loaded page copy
    ├── mmap page copy
    └── 전체 cleanup 안정화
```

---

# 14. 최종 결론

Project 3는 기능이 많아 보이지만, 결국 하나의 흐름으로 이어진다.

```text
SPT에 page 설명서를 등록한다.
page fault가 나면 SPT에서 page를 찾는다.
frame을 확보하고 page table에 매핑한다.
page 타입에 맞게 내용을 frame에 채운다.
메모리가 부족하면 eviction과 swap으로 frame을 재사용한다.
fork와 exit에서는 SPT와 자원을 복사하거나 정리한다.
```

팀 구현에서는 기능을 따로따로 보는 것보다, 매번 아래 질문을 붙잡고 가는 것이 좋다.

```text
이 함수는 전체 흐름 중 어디에 붙는가?
이 구조체는 어떤 정보를 어디에 저장하는가?
실패하면 무엇을 되돌려야 하는가?
다른 팀원의 코드와 어디에서 만나는가?
```
