# Pintos Project 3 VM End-to-End Flow

## 1. 한 줄 요약

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

## 2. 전체 구조도

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
                └── vm_try_handle_fault()
                    ├── 잘못된 주소
                    │   └── false 반환
                    │       └── 프로세스 종료
                    │
                    ├── stack growth 가능
                    │   └── vm_stack_growth()
                    │       └── 새 anonymous page 할당
                    │           └── page claim
                    │
                    └── 일반 lazy page
                        └── spt_find_page()
                            ├── page 없음
                            │   └── false 반환
                            │       └── 프로세스 종료
                            │
                            └── page 있음
                                └── vm_do_claim_page()
                                    ├── vm_get_frame()
                                    │   ├── 빈 frame 있음
                                    │   │   └── palloc_get_page(PAL_USER)
                                    │   │       └── struct frame 생성
                                    │   │           └── frame_table 등록
                                    │   │
                                    │   └── 빈 frame 없음
                                    │       └── vm_evict_frame()
                                    │           └── victim frame 선정
                                    │               └── swap_out()
                                    │
                                    ├── page와 frame 연결
                                    │   ├── page->frame = frame
                                    │   └── frame->page = page
                                    │
                                    ├── page table 매핑
                                    │   └── pml4_set_page()
                                    │       └── page->va -> frame->kva
                                    │
                                    └── swap_in()
                                        ├── VM_UNINIT
                                        │   └── uninit_initialize()
                                        │       ├── anon_initializer()
                                        │       ├── file_backed_initializer()
                                        │       └── init(page, aux)
                                        │           └── lazy_load_segment()
                                        │
                                        ├── VM_ANON
                                        │   └── anon_swap_in()
                                        │       └── swap disk에서 복원
                                        │
                                        └── VM_FILE
                                            └── file_backed_swap_in()
                                                └── backing file에서 읽기
```

## 3. page 등록 흐름

page fault가 나기 전에 page 정보가 먼저 SPT에 등록된다.

```text
process load / mmap / stack setup
└── vm_alloc_page_with_initializer()
    ├── struct page 생성
    ├── uninit page로 초기화
    ├── 나중에 될 타입 저장
    │   ├── VM_ANON
    │   └── VM_FILE
    ├── init 함수와 aux 저장
    └── SPT에 삽입
```

이 시점에는 아직 실제 frame이 붙지 않는다.

```text
page->frame == NULL
```

## 4. page fault 복구 흐름

page fault가 발생하면 커널은 먼저 이 fault가 복구 가능한지 판단한다.

```text
page fault
└── vm_try_handle_fault()
    ├── addr가 NULL 또는 kernel address면 실패
    ├── not_present fault가 아니면 실패
    ├── SPT에서 page를 못 찾으면 실패
    ├── write fault인데 page가 writable이 아니면 실패
    └── 통과하면 vm_do_claim_page()
```

## 5. frame 확보와 page table 매핑

SPT에서 page를 찾았다면 실제 물리 frame을 확보한다.

```text
vm_do_claim_page(page)
└── vm_get_frame()
    ├── palloc_get_page(PAL_USER)
    ├── struct frame 생성
    ├── frame->kva 저장
    ├── frame->page = NULL
    └── frame_table에 등록
```

그 다음 page와 frame을 연결한다.

```text
page->frame = frame
frame->page = page
```

그리고 page table에 매핑한다.

```text
pml4_set_page(
    thread_current()->pml4,
    page->va,
    frame->kva,
    page->writable
)
```

이 매핑이 성공해야 CPU가 다음 접근에서 page fault 없이 메모리에 접근할 수 있다.

## 6. 타입별 swap_in 흐름

`swap_in(page, frame->kva)`는 page 타입에 따라 다른 함수로 연결된다.

```text
swap_in(page, kva)
└── page->operations->swap_in(page, kva)
```

### VM_UNINIT

처음 접근하는 lazy page다.

```text
uninit_initialize()
├── page->uninit.type 확인
├── page->uninit.page_initializer 실행
│   ├── anon_initializer()
│   └── file_backed_initializer()
└── init(page, aux) 실행
    └── lazy_load_segment()
```

### VM_ANON

파일 원본이 없는 page다.

```text
anon_swap_in()
└── swap out된 적이 있다면 swap disk에서 복원
```

### VM_FILE

파일을 backing store로 가지는 page다.

```text
file_backed_swap_in()
└── 파일의 offset 위치에서 데이터를 읽어 frame->kva에 채움
```

## 7. 메모리가 부족할 때

user pool에 빈 frame이 없으면 eviction이 필요하다.

```text
vm_get_frame()
└── palloc_get_page(PAL_USER) 실패
    └── vm_evict_frame()
        ├── vm_get_victim()
        ├── victim page swap_out()
        ├── page table 매핑 제거
        ├── page-frame 연결 해제
        └── 비워진 frame 재사용
```

page 타입별로 swap_out 방식이 다르다.

```text
VM_ANON
└── swap disk에 저장

VM_FILE
├── dirty면 파일에 write-back
└── dirty가 아니면 다시 파일에서 읽으면 되므로 버릴 수 있음
```

## 8. fork와 exit 정리

프로세스마다 SPT를 가지므로 fork와 exit에서도 SPT를 처리해야 한다.

```text
fork
└── supplemental_page_table_copy()
    ├── uninit page 복사
    ├── loaded anon page 복사
    └── file-backed page 복사
```

```text
exit / munmap
└── supplemental_page_table_kill() 또는 do_munmap()
    ├── dirty file-backed page write-back
    ├── swap slot 해제
    ├── page table mapping 제거
    ├── frame 정리
    └── page destroy
```

## 9. 최종 흐름 압축

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

## 10. 전체 통합 흐름과 머지 위치

아래 구조도는 Project 3 흐름을 처음부터 끝까지 하나로 합친 버전이다.  
각 단계 오른쪽의 `Merge N`은 해당 기능을 어느 머지에서 구현하거나 완성하는지 나타낸다.

```text
Project 3 VM Integrated Flow

0. 공통 개념 정리                                            [Merge 0]
   ├── page / frame / kva / page table
   ├── SPT와 page table의 차이
   └── page fault가 복구 가능한 상황인지 이해

1. 프로그램 load 또는 mmap 호출                              [Merge 1, Merge 3]
   ├── 실행 파일 segment 분석                                [Merge 1]
   ├── mmap 인자 검증                                        [Merge 3]
   └── 필요한 가상 주소 범위 계산

2. page 설명서 생성                                           [Merge 1, Merge 3]
   ├── vm_alloc_page_with_initializer()                       [Merge 1]
   ├── uninit page 생성                                       [Merge 1]
   ├── future type 저장
   │   ├── VM_ANON                                            [Merge 1]
   │   └── VM_FILE                                            [Merge 1, Merge 3]
   ├── init 함수와 aux 저장                                   [Merge 1, Merge 3]
   └── SPT에 page 등록                                        [SPT 공통 기반]

3. 유저 프로그램 실행                                         [Merge 1]
   └── user virtual address 접근

4. page table 확인                                            [Merge 1]
   ├── 매핑 있음
   │   └── 바로 physical frame 접근
   │       └── 유저 프로그램 계속 실행
   │
   └── 매핑 없음 또는 권한 문제
       └── page fault 발생

5. page fault 처리 진입                                       [Merge 1, Merge 2]
   └── vm_try_handle_fault()
       ├── NULL 주소 검사                                     [Merge 1]
       ├── kernel address 검사                                [Merge 1]
       ├── not_present fault 검사                             [Merge 1]
       ├── write 접근과 writable 검사                         [Merge 1]
       └── stack growth 가능성 검사                           [Merge 2]

6. stack growth 분기                                          [Merge 2]
   ├── stack 접근으로 판단됨
   │   └── vm_stack_growth()
   │       ├── addr page align
   │       ├── 1MB stack limit 확인
   │       ├── VM_ANON page 할당
   │       └── page claim
   │
   └── stack 접근 아님
       └── 일반 lazy page 처리로 진행

7. SPT에서 page 찾기                                          [SPT 공통 기반, Merge 1]
   └── spt_find_page()
       ├── page 없음
       │   └── invalid access
       │       └── 프로세스 종료
       │
       └── page 있음
           └── vm_do_claim_page(page)

8. frame 확보                                                 [Merge 1, Merge 4]
   └── vm_get_frame()
       ├── free frame 있음
       │   ├── palloc_get_page(PAL_USER)                      [Merge 1]
       │   ├── struct frame 생성                              [Merge 1]
       │   └── frame_table 등록                               [Merge 1]
       │
       └── free frame 없음
           └── vm_evict_frame()                               [Merge 4]
               ├── vm_get_victim()                            [Merge 4]
               ├── accessed bit 기반 victim 선정              [Merge 4]
               ├── victim page swap_out()                     [Merge 4]
               ├── page table mapping 제거                    [Merge 4]
               └── 비워진 frame 재사용                        [Merge 4]

9. page와 frame 연결                                          [Merge 1]
   ├── page->frame = frame
   └── frame->page = page

10. page table 매핑                                           [Merge 1]
    └── pml4_set_page()
        ├── page->va
        ├── frame->kva
        └── page->writable

11. page 타입별 swap_in                                       [Merge 1, Merge 3, Merge 4]
    └── swap_in(page, frame->kva)
        ├── VM_UNINIT                                         [Merge 1]
        │   └── uninit_initialize()
        │       ├── anon_initializer()                        [Merge 1]
        │       ├── file_backed_initializer()                 [Merge 1, Merge 3]
        │       └── init(page, aux)
        │           └── lazy_load_segment()                   [Merge 1]
        │
        ├── VM_ANON                                           [Merge 1, Merge 4]
        │   └── anon_swap_in()
        │       ├── 처음 접근이면 복원할 내용 없음            [Merge 1]
        │       └── swap out된 page면 swap disk에서 복원       [Merge 4]
        │
        └── VM_FILE                                           [Merge 3, Merge 4]
            └── file_backed_swap_in()
                ├── file offset 위치에서 읽기                 [Merge 3]
                ├── read_bytes만큼 frame에 채우기              [Merge 3]
                └── 남은 zero_bytes 0으로 채우기               [Merge 3]

12. fault 복구 완료                                           [Merge 1]
    ├── true 반환
    ├── fault가 났던 명령 재실행
    └── 이번에는 page table 매핑이 있으므로 실행 계속

13. mmap 해제                                                 [Merge 3]
    └── munmap()
        ├── mmap page 범위 순회
        ├── dirty page write-back
        ├── page table mapping 제거
        └── SPT에서 page 제거

14. swap_out                                                   [Merge 4]
    ├── VM_ANON
    │   ├── swap slot 확보
    │   ├── frame 내용을 swap disk에 기록
    │   └── page에 swap slot 정보 저장
    │
    └── VM_FILE
        ├── dirty bit 확인
        ├── dirty면 backing file에 기록
        └── dirty 아니면 기록 없이 frame만 비움

15. fork                                                       [Merge 5]
    └── supplemental_page_table_copy()
        ├── uninit page 복사                                  [Merge 5]
        ├── loaded anon page 복사                             [Merge 5]
        ├── file-backed page 복사                             [Merge 5]
        └── 자식 SPT에 등록

16. exit                                                       [Merge 2, Merge 5]
    └── supplemental_page_table_kill()
        ├── 모든 page 순회                                    [Merge 2]
        ├── dirty file-backed page write-back                 [Merge 5]
        ├── swap slot 해제                                    [Merge 5]
        ├── frame 정리                                        [Merge 5]
        ├── destroy(page)                                     [Merge 2]
        └── page free
```

## 11. 머지 기준으로 다시 보기

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

## 12. 머지 차시별 A/B/C/D 역할

### Merge 1 - Frame Claim + Lazy Loading 기본 흐름

목표:

```text
page fault가 났을 때 SPT에서 찾은 page를 실제 frame에 올리고,
page table에 매핑한 뒤, lazy loading으로 내용을 채운다.
```

이상적인 머지 순서:

```text
1. B - Uninit Page와 Initializer
2. A - Frame Claim
3. C - Executable Segment Lazy Loading
4. D - 초기 Stack Page
```

이유:

```text
B가 page의 타입 전환과 swap_in 연결을 먼저 잡아야 A의 claim 흐름이 끝까지 이어진다.
A가 frame과 page table 매핑을 잡으면 C/D가 실제 page를 올릴 수 있다.
C는 실행 파일 lazy loading, D는 초기 stack이라 A/B 이후에 붙이는 것이 안정적이다.
```

#### A. Frame Claim

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

#### B. Uninit Page와 Initializer

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

#### C. Executable Segment Lazy Loading

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

#### D. 초기 Stack Page

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

### Merge 2 - Stack Growth + Page Cleanup 기본 흐름

목표:

```text
stack이 커지는 접근을 page fault에서 구분하고,
프로세스 종료 시 page 자원을 안전하게 정리한다.
```

이상적인 머지 순서:

```text
1. C - Page Destroy
2. D - SPT Kill
3. B - Stack Growth 실행
4. A - Stack Growth 판별
```

이유:

```text
C/D의 cleanup 기반이 먼저 있어야 새로 만든 stack page도 종료 시 안전하게 정리된다.
B가 실제 stack page 생성 함수를 만든 뒤, A가 fault handler에서 그 함수를 호출하는 흐름이 자연스럽다.
A는 page fault 입구를 건드리므로 가장 마지막에 붙이는 것이 충돌과 회귀를 줄인다.
```

#### A. Stack Growth 판별

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

#### B. Stack Growth 실행

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

#### C. Page Destroy

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

#### D. SPT Kill

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

### Merge 3 - mmap / File-backed Page 기본 흐름

목표:

```text
파일을 유저 주소 공간에 매핑하고,
page fault 시 파일 내용을 frame으로 읽어온다.
```

이상적인 머지 순서:

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

#### A. mmap Validation

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

#### B. mmap Page Registration

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

#### C. File-backed Swap In

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

#### D. munmap과 Write-back

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

### Merge 4 - Eviction + Swap In/Out

목표:

```text
물리 frame이 부족할 때 기존 page를 안전하게 내보내고,
비워진 frame을 재사용한다.
```

이상적인 머지 순서:

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

#### A. Eviction Victim Selection

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

#### B. Eviction Flow

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

#### C. Anonymous Swap Table

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

#### D. File-backed Swap Out

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

### Merge 5 - Fork/Exit 정리 + 전체 안정화

목표:

```text
fork와 exit에서도 SPT/page/frame/file/swap 자원이 일관되게 복사되고 정리되도록 마무리한다.
```

이상적인 머지 순서:

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

#### A. SPT Copy - Uninit Page

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

#### B. SPT Copy - Loaded Anon Page

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

#### C. SPT Copy - File-backed/mmap Page

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

#### D. Exit와 전체 Cleanup 안정화

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
