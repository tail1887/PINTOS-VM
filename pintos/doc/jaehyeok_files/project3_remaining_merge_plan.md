# Pintos Project 3 남은 구현 범위와 머지 계획

## 목차

1. 가상 메모리 전체 흐름
   - Merge 0 / 공통: A,B,C,D 모두 page fault 복구 흐름과 각 머지 목표를 맞춘다.

2. Page와 Frame 기본 구조
   - Merge 0 / 공통: A,B,C,D 모두 `page`, `frame`, `kva`, page table 관계를 같은 용어로 정리한다.

3. Frame Claim
   - Merge 1 / A: `vm_get_frame`, `vm_claim_page`, `vm_do_claim_page`, page table 매핑을 담당한다.

4. Uninit Page와 Lazy Loading
   - Merge 1 / B: `vm_alloc_page_with_initializer`, `uninit_initialize`, anon/file initializer를 담당한다.

5. Executable Segment Lazy Loading
   - Merge 1 / C: `load_segment`, `lazy_load_segment`, segment용 aux와 offset 처리를 담당한다.

6. 초기 Stack Page
   - Merge 1 / D: `setup_stack`, 초기 stack page 할당, argument passing 연결을 담당한다.

7. Stack Growth 판별
   - Merge 2 / A: `vm_try_handle_fault`에서 stack growth 가능 조건과 `rsp` 기준 검사를 담당한다.

8. Stack Growth 실행
   - Merge 2 / B: `vm_stack_growth`, 새 anonymous page 할당, 1MB 제한 적용을 담당한다.

9. Page Destroy
   - Merge 2 / C: `uninit_destroy`, `anon_destroy`, page 타입별 destroy 흐름을 담당한다.

10. SPT Kill
    - Merge 2 / D: `supplemental_page_table_kill`, SPT 전체 순회와 process exit cleanup을 담당한다.

11. mmap Validation
    - Merge 3 / A: `do_mmap` 인자 검증, 주소 정렬, fd, length, overlap 검사를 담당한다.

12. mmap Page Registration
    - Merge 3 / B: mmap 영역을 page 단위로 나누고 file-backed page 등록을 담당한다.

13. File-backed Swap In
    - Merge 3 / C: `file_backed_initializer`, `file_backed_swap_in`, 파일에서 frame으로 읽기를 담당한다.

14. munmap과 Write-back
    - Merge 3 / D: `do_munmap`, dirty page write-back, mapping 제거를 담당한다.

15. Eviction Victim Selection
    - Merge 4 / A: `vm_get_victim`, frame table 순회, accessed bit 기반 victim 선정을 담당한다.

16. Eviction Flow
    - Merge 4 / B: `vm_evict_frame`, `swap_out`, `pml4_clear_page`, frame 재사용 흐름을 담당한다.

17. Anonymous Swap Table
    - Merge 4 / C: `vm_anon_init`, swap disk, swap slot bitmap, anon swap in/out을 담당한다.

18. File-backed Swap Out
    - Merge 4 / D: `file_backed_swap_out`, dirty bit 확인, 파일 write-back을 담당한다.

19. SPT Copy - Uninit Page
    - Merge 5 / A: fork 시 uninit page의 type, writable, init, aux 복사를 담당한다.

20. SPT Copy - Loaded Anon Page
    - Merge 5 / B: 이미 frame에 올라온 anon page claim과 frame 내용 복사를 담당한다.

21. SPT Copy - File-backed/mmap Page
    - Merge 5 / C: file-backed page와 mmap page의 backing 정보를 자식에게 복사하는 흐름을 담당한다.

22. Exit와 전체 Cleanup 안정화
    - Merge 5 / D: exit, munmap, swap cleanup 중복 해제 방지와 전체 정리를 담당한다.

## 한눈에 보는 목차

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

## 오늘 회의에서 정할 것

```text
[ ] 오늘 구현할 Merge 단위
[ ] 4명 역할 분배
[ ] 오늘의 완료 기준
[ ] 머지 후 코드 리뷰 방식
[ ] 내일 이어갈 범위
```

## 진행률 표시

```text
전체 머지 5개 중 0개 완료

Merge 1: [ ] Frame Claim + Lazy Loading
Merge 2: [ ] Stack Growth + Page Cleanup
Merge 3: [ ] mmap / File-backed Page
Merge 4: [ ] Eviction + Swap In/Out
Merge 5: [ ] Fork/Exit 정리 + 전체 안정화
```

## 0. 전제

SPT 기본 구현은 공통 기반으로 보고, 이 문서에서는 SPT를 제외한 나머지 Project 3 구현 범위를 다룬다.

남은 기간은 약 8일로 가정한다.

목표는 다음과 같다.

```text
1. 기능과 개념을 순서대로 학습한다.
2. 4명 모두 비슷한 양과 난이도의 구현을 맡는다.
3. 테스트 검증만 담당하는 사람을 두지 않는다.
4. 각 머지마다 작동 가능한 단위를 만든다.
5. 머지 후에는 전원이 코드 설명을 듣고 다음 단계로 넘어간다.
```

## 1. Project 3 전체 흐름

SPT 이후의 큰 흐름은 다음과 같다.

```text
유저가 va 접근
-> page table에 매핑 없음
-> page fault 발생
-> SPT에서 page 정보 확인
-> frame 확보
-> page 타입에 맞게 내용 로드
-> page table에 va -> frame 매핑
-> 유저 프로그램 계속 실행
```

SPT 이후에 구현할 기능은 이 흐름을 완성하기 위한 부품들이다.

```text
frame management
lazy loading
anonymous page
file-backed page
stack growth
mmap / munmap
swap in / swap out
process fork / exit cleanup
```

## 2. 추천 머지 횟수

8일 기준으로는 **5번의 머지**를 추천한다.

```text
Merge 1: frame claim + lazy loading 기본 흐름
Merge 2: stack growth + page cleanup 기본 흐름
Merge 3: mmap / file-backed page 기본 흐름
Merge 4: eviction + swap in/out
Merge 5: fork/exit 정리 + 전체 안정화
```

5번보다 적으면 한 번의 머지가 너무 커진다.  
5번보다 많으면 회의와 충돌 해결 비용이 커질 수 있다.

## 3. 학습 순서

구현 전에 다 같이 볼 순서는 다음이 좋다.

```text
1. page / frame / page table / kva 복습
2. frame allocation과 vm_do_claim_page
3. uninit page와 lazy loading
4. anonymous page
5. stack growth
6. file-backed page와 mmap
7. swap in/out과 eviction
8. fork/exit에서 SPT copy/kill과 destroy
```

이 순서를 지키면 앞 단계에서 만든 흐름 위에 다음 기능을 얹을 수 있다.

## 4. Merge 1 - Frame Claim + Lazy Loading 기본 흐름

### 목표

page fault가 났을 때 SPT에서 찾은 page를 실제 frame에 올릴 수 있게 한다.

```text
page fault
-> frame 확보
-> page와 frame 연결
-> page table 매핑
-> uninit page를 anon/file page로 전환
-> lazy load 실행
```

### 학습 키워드

```text
frame
kva
palloc_get_page(PAL_USER)
pml4_set_page
swap_in 매크로
uninit_initialize
vm_alloc_page_with_initializer
lazy_load_segment
```

### 4인 구현 분배

#### 1팀원 - frame claim 흐름

구현 범위:

```text
vm_get_frame
vm_claim_page
vm_do_claim_page
pml4_set_page 실패 처리
swap_in 실패 시 정리 흐름 초안
```

학습 포인트:

```text
struct frame과 실제 물리 frame의 차이
frame->kva의 의미
page table 매핑이 왜 필요한지
```

#### 2팀원 - uninit page와 initializer 흐름

구현 범위:

```text
vm_alloc_page_with_initializer
uninit_initialize
anon_initializer
file_backed_initializer
```

학습 포인트:

```text
uninit page가 왜 필요한지
page->operations가 어떻게 타입별 함수를 고르는지
함수 포인터가 왜 쓰이는지
```

#### 3팀원 - executable lazy loading

구현 범위:

```text
process.c load_segment
lazy_load_segment
segment 로딩용 aux 구조체
파일 offset/read_bytes/zero_bytes 처리
```

학습 포인트:

```text
왜 실행 파일을 한 번에 다 읽지 않는지
offset이 무엇인지
page fault 시점에 파일 내용을 읽는 흐름
```

#### 4팀원 - setup_stack과 초기 stack page

구현 범위:

```text
setup_stack
초기 stack page 할당
vm_claim_page를 통한 stack frame 연결
argument passing과 stack page 연결 확인
```

학습 포인트:

```text
user stack 위치
stack page도 SPT/page/frame 흐름에 들어가는지
Project 2 argument passing과 Project 3 stack의 연결
```

### Merge 1 완료 기준

```text
빌드 성공
기본 user program 실행 가능
lazy-anon 또는 lazy-file 쪽 진입 흐름 확인
Project 2 기본 테스트가 크게 깨지지 않는지 확인
```

## 5. Merge 2 - Stack Growth + Page Cleanup 기본 흐름

### 목표

유저 stack이 현재 page보다 더 커질 때, 올바른 접근이면 새 page를 할당한다.  
프로세스 종료 시 page 자원을 정리하는 흐름도 같이 잡는다.

### 학습 키워드

```text
stack pointer rsp
USER_STACK
stack growth heuristic
1MB stack limit
destroy(page)
anon_destroy
uninit_destroy
```

### 4인 구현 분배

#### 1팀원 - stack growth 판별

구현 범위:

```text
vm_try_handle_fault에서 stack growth 조건 검사
rsp 기준 접근 허용 범위 판단
kernel mode fault에서 user rsp 저장 흐름 확인
```

학습 포인트:

```text
page fault가 stack growth인지 아닌지 구분하는 기준
PUSH 명령이 rsp 아래 8바이트에서 fault를 낼 수 있는 이유
```

#### 2팀원 - vm_stack_growth

구현 범위:

```text
vm_stack_growth
addr page align
VM_ANON page 할당
claim
1MB stack limit 적용
```

학습 포인트:

```text
stack이 높은 주소에서 낮은 주소로 자라는 구조
stack page를 anonymous page로 보는 이유
```

#### 3팀원 - page destroy 흐름

구현 범위:

```text
uninit_destroy
anon_destroy
vm_dealloc_page 흐름 확인
frame과 연결된 page 정리 기준 확인
```

학습 포인트:

```text
free(page) 전에 destroy(page)를 호출하는 이유
page 타입별 정리가 필요한 이유
```

#### 4팀원 - supplemental_page_table_kill

구현 범위:

```text
supplemental_page_table_kill
SPT 전체 순회
각 page destroy
hash destroy 흐름
process_exit와 연결 확인
```

학습 포인트:

```text
프로세스 종료 시 SPT가 왜 필요한지
eviction과 page 삭제의 차이
```

### Merge 2 완료 기준

```text
stack growth 관련 테스트 일부 통과 기대
프로세스 종료 시 page cleanup 흐름 확인
page fault가 stack인지 invalid access인지 구분 가능
```

## 6. Merge 3 - mmap / File-backed Page 기본 흐름

### 목표

파일을 유저 가상 주소 공간에 mapping하고, page fault 시 파일 내용을 frame으로 읽어온다.

### 학습 키워드

```text
mmap
munmap
file-backed page
file_reopen
offset
read_bytes
zero_bytes
dirty bit
```

### 4인 구현 분배

#### 1팀원 - mmap validation

구현 범위:

```text
do_mmap 기본 인자 검증
addr page align 검사
length 0 검사
fd 0/1 거부
기존 page와 overlap 검사
```

학습 포인트:

```text
mmap이 실패해야 하는 조건
기존 stack/code/data 영역과 겹치면 안 되는 이유
```

#### 2팀원 - mmap page 등록

구현 범위:

```text
do_mmap에서 page 단위 반복 등록
VM_FILE page 생성
mmap용 aux 구성
file offset/read_bytes/zero_bytes 계산
```

학습 포인트:

```text
파일 하나가 여러 page에 나뉘어 매핑되는 방식
마지막 page에서 zero_bytes가 필요한 이유
```

#### 3팀원 - file-backed swap_in

구현 범위:

```text
file_backed_initializer
file_backed_swap_in
파일에서 kva로 읽기
남은 영역 zero fill
```

학습 포인트:

```text
file-backed page가 파일을 backing store로 쓰는 방식
lazy loading과 mmap loading의 공통점
```

#### 4팀원 - munmap과 write-back

구현 범위:

```text
do_munmap
dirty page write-back
pml4 dirty bit 확인
mapping 제거
file close 기준 확인
```

학습 포인트:

```text
dirty bit가 왜 필요한지
수정한 page만 파일에 다시 써야 하는 이유
```

### Merge 3 완료 기준

```text
mmap-read 계열 일부 통과 기대
mmap validation 테스트 일부 통과 기대
munmap 시 수정 내용 write-back 흐름 확인
```

## 7. Merge 4 - Eviction + Swap In/Out

### 목표

user pool에 빈 frame이 없을 때 기존 frame 하나를 쫓아내고 재사용한다.

### 학습 키워드

```text
eviction
victim frame
accessed bit
dirty bit
swap disk
swap slot
bitmap
anon_swap_in
anon_swap_out
file_backed_swap_out
```

### 4인 구현 분배

#### 1팀원 - victim 선정

구현 범위:

```text
vm_get_victim
frame_table 순회
accessed bit 기반 second chance 정책
```

학습 포인트:

```text
왜 아무 frame이나 쫓아내면 안 되는지
accessed bit가 최근 사용 여부를 알려주는 방식
```

#### 2팀원 - eviction 흐름

구현 범위:

```text
vm_evict_frame
victim page swap_out 호출
pml4_clear_page
page-frame 연결 해제
frame 재사용
```

학습 포인트:

```text
eviction은 page 삭제가 아니라 frame에서 내리는 것
SPT의 page는 남아 있어야 하는 이유
```

#### 3팀원 - anonymous swap table

구현 범위:

```text
vm_anon_init
swap disk block 가져오기
swap slot bitmap
anon_page에 swap slot 정보 추가
anon_swap_out
anon_swap_in
```

학습 포인트:

```text
anon page는 파일 원본이 없기 때문에 swap disk가 필요한 이유
swap slot이 page-size 단위인 이유
```

#### 4팀원 - file-backed eviction

구현 범위:

```text
file_backed_swap_out
dirty page면 파일 write-back
dirty 아니면 discard
file_backed_destroy 보강
```

학습 포인트:

```text
file-backed page는 원본 파일이 backing store가 되는 이유
dirty bit가 false면 다시 읽으면 되는 이유
```

### Merge 4 완료 기준

```text
swap-anon 일부 통과 기대
swap-file 일부 통과 기대
메모리 제한이 있는 테스트에서 eviction 경로 진입 확인
```

## 8. Merge 5 - Fork/Exit 정리 + 전체 안정화

### 목표

fork, exit, mmap cleanup, swap cleanup까지 연결해서 전체 테스트 통과 가능성을 높인다.

### 학습 키워드

```text
supplemental_page_table_copy
loaded page copy
uninit page copy
fork
process_exit
resource cleanup
double free 방지
```

### 4인 구현 분배

#### 1팀원 - SPT copy: uninit page

구현 범위:

```text
supplemental_page_table_copy에서 uninit page 복사
type/writable/init/aux 보존
자식 SPT에 page 등록
```

학습 포인트:

```text
부모와 자식이 같은 struct page를 공유하면 안 되는 이유
lazy page를 fork할 때 어떻게 복사할지
```

#### 2팀원 - SPT copy: loaded anon page

구현 범위:

```text
이미 frame에 올라온 anon page 복사
자식 page claim
frame 내용 복사
```

학습 포인트:

```text
fork 후 부모와 자식 메모리가 독립적이어야 하는 이유
Copy-on-write를 제외하면 즉시 복사가 필요한 이유
```

#### 3팀원 - SPT copy: file-backed/mmap page

구현 범위:

```text
file-backed page 복사 정책
mmap page 정보 보존
file_reopen 기준 확인
mmap-inherit 관련 흐름 점검
```

학습 포인트:

```text
파일 backing 정보를 자식에게 어떻게 넘길지
fd 복사와 file-backed page 복사의 차이
```

#### 4팀원 - cleanup 안정화

구현 범위:

```text
supplemental_page_table_kill 보강
munmap과 exit cleanup 중복 정리 방지
swap slot 해제
frame_table 제거
```

학습 포인트:

```text
자원 해제 순서
double free가 생기는 경우
프로세스 종료 시 page/frame/swap/file이 어떻게 정리되는지
```

### Merge 5 완료 기준

```text
fork 관련 VM 테스트 일부 통과 기대
mmap-exit, mmap-inherit 계열 점검
swap-fork 계열 점검
전체 vm 테스트 회귀 확인
```

## 9. 8일 운영안

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

## 10. 머지마다 공통 리뷰 기준

각 머지마다 아래 질문에 답하고 넘어간다.

```text
1. 이 기능은 전체 흐름 중 어느 단계인가?
2. 새로 만든 구조체 필드는 어디에 저장되는 정보인가?
3. 이 함수는 page를 다루는가, frame을 다루는가, page table을 다루는가?
4. 실패하면 어떤 자원을 정리해야 하는가?
5. 이 머지로 통과 가능성이 생긴 테스트는 무엇인가?
6. 다음 머지에서 이어받아야 하는 TODO는 무엇인가?
```

## 11. 추천 결론

이번 팀 구현은 처음부터 기능별로 완전히 쪼개기보다, **머지 단위는 함께 정하고 각 머지 안에서 4명이 비슷한 난이도의 하위 기능을 나누는 방식**이 좋다.

특히 Project 3는 모든 기능이 다음 흐름으로 연결된다.

```text
SPT
-> page fault
-> frame
-> page table mapping
-> swap_in
-> stack/mmap/swap/fork cleanup
```

따라서 매번 머지 후에는 “내 코드만 설명”이 아니라 “전체 흐름에서 어디에 붙는지”를 서로 설명해야 한다.
