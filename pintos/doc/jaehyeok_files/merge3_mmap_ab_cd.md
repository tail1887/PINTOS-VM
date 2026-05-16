# Pintos Project 3 VM - Merge 3 mmap AB/CD 학습 정리

## 1. 전체 흐름 먼저 보기

Merge 3의 주제는 `mmap`과 `munmap`이다.

한 문장으로 말하면 다음과 같다.

```text
파일의 내용을 유저 가상 주소 공간에 연결해두고,
처음 접근할 때 실제로 파일 내용을 frame에 읽어오며,
해제할 때 수정된 내용이 있으면 다시 파일에 반영하는 기능
```

전체 흐름은 다음과 같다.

```text
1. 유저 프로그램이 mmap(addr, length, writable, fd, offset)을 호출한다.

2. syscall_handler가 SYS_MMAP을 받아 커널의 do_mmap()으로 넘긴다.

3. do_mmap()이 요청이 유효한지 검사한다.
   - addr이 NULL이 아닌가?
   - addr이 page aligned인가?
   - length가 0보다 큰가?
   - offset이 page aligned인가?
   - fd가 유효한 파일인가?
   - 매핑하려는 주소 범위가 기존 page와 겹치지 않는가?

4. 유효하면 파일 구간을 page 단위로 나눈다.
   - page_read_bytes
   - page_zero_bytes
   - file offset
   - writable

5. 각 page를 VM_FILE 타입으로 SPT에 등록한다.
   - vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_load 함수, aux)
   - aux에는 file, offset, read_bytes, zero_bytes 같은 정보를 담는다.

6. 이 시점에는 아직 frame에 파일 내용이 올라오지 않았을 수 있다.
   - lazy loading 상태
   - SPT에는 page 정보만 있음

7. 유저가 mmap된 주소에 처음 접근한다.
   - page fault 발생
   - vm_try_handle_fault()
   - SPT에서 VM_FILE page 찾기
   - vm_do_claim_page()
   - frame 확보
   - page table 매핑
   - swap_in(page, frame->kva)
   - file_backed_swap_in()

8. file_backed_swap_in()이 파일 내용을 frame에 읽는다.
   - file_read_at()
   - 나머지 영역은 0으로 채움

9. 유저가 mmap 영역을 읽거나 쓴다.
   - 읽기 mmap이면 읽기만 가능
   - 쓰기 mmap이면 page table dirty bit가 켜질 수 있음

10. 유저가 munmap(addr)을 호출하거나 프로세스가 종료된다.

11. do_munmap()이 mmap된 page들을 순회한다.
    - page가 dirty인지 확인
    - dirty면 file_write_at()으로 파일에 write-back
    - SPT에서 page 제거
    - frame/page 자원 정리
```

초간단 그림은 다음과 같다.

```text
mmap()
└── 파일을 메모리 주소에 예약 등록
    └── SPT에 VM_FILE page 등록

첫 접근
└── page fault
    └── file_backed_swap_in()
        └── 파일 내용을 frame에 읽음

munmap()
└── dirty page write-back
    └── page 제거
```

## 2. AB/CD 2조 분배

Merge 3를 2조로 나누면 다음이 가장 자연스럽다.

```text
AB팀: mmap 검증 + page 등록
CD팀: file-backed page 동작 + munmap/write-back
```

## 3. AB팀 범위

AB팀은 mmap의 입구를 맡는다.

```text
A: mmap validation
B: mmap page registration
```

AB팀의 핵심 질문은 다음이다.

```text
이 mmap 요청을 받아도 되는가?
받아도 된다면 어떤 page들을 SPT에 등록해야 하는가?
```

AB팀 담당 함수와 위치는 다음과 같다.

```text
pintos/userprog/syscall.c
- SYS_MMAP 분기 추가
- SYS_MUNMAP 분기 추가 가능
- fd로 struct file * 찾기

pintos/vm/file.c
- do_mmap()
- mmap page 등록

pintos/include/vm/file.h
- file_page 구조체 필드 추가
- 필요하면 mmap aux 구조체 관련 선언
```

## 4. CD팀 범위

CD팀은 mmap page가 실제로 동작하고 해제되는 부분을 맡는다.

```text
C: file-backed swap in
D: munmap + write-back
```

CD팀의 핵심 질문은 다음이다.

```text
SPT에 등록된 VM_FILE page가 처음 접근될 때 무엇을 frame에 채워야 하는가?
munmap 때 수정된 내용을 파일에 어떻게 반영하고 page를 제거할 것인가?
```

CD팀 담당 함수와 위치는 다음과 같다.

```text
pintos/vm/file.c
- file_backed_initializer()
- file_backed_swap_in()
- file_backed_swap_out()
- file_backed_destroy()
- do_munmap()

pintos/include/vm/file.h
- struct file_page 필드 추가

pintos/vm/vm.c
- supplemental_page_table_kill()과의 연결 확인
```

## 5. mmap이 무엇인가?

`mmap`은 memory mapped file의 줄임말이다.

파일을 `read()`로 직접 읽어 버퍼에 복사하는 방식이 아니라, 파일을 유저 가상 메모리 주소에 연결해두는 기능이다.

유저 입장에서는 다음처럼 보인다.

```text
파일을 열고
그 파일을 특정 메모리 주소에 붙이고
그 주소를 배열처럼 읽거나 쓴다.
```

예를 들어:

```c
void *map = mmap((void *) 0x10000000, 4096, 1, fd, 0);
```

이 뜻은 다음과 같다.

```text
fd가 가리키는 파일의 offset 0부터
4096바이트를
유저 주소 0x10000000부터 연결하고
쓰기 가능하게 하겠다.
```

하지만 mmap 호출 순간 파일 내용을 바로 전부 읽는 것은 아니다.

Project 3에서는 lazy loading 구조를 사용한다.

```text
mmap 호출
-> SPT에 VM_FILE page 등록만 함

처음 접근
-> page fault
-> frame 확보
-> 파일 내용을 frame에 읽음
```

## 6. mmap과 read의 차이

`read()`는 호출 순간 파일 내용을 버퍼에 복사한다.

```text
read(fd, buffer, size)
-> 지금 바로 파일 내용을 buffer에 복사
```

`mmap()`은 파일과 메모리 주소를 연결한다.

```text
mmap(addr, length, writable, fd, offset)
-> 파일을 addr부터의 메모리 영역에 연결
-> 실제 읽기는 처음 접근할 때 lazy하게 수행
```

차이는 다음과 같다.

```text
read
- 즉시 복사
- buffer는 그냥 일반 메모리
- 파일과 계속 연결되어 있지 않음

mmap
- 메모리 주소와 파일이 연결됨
- 처음 접근 때 파일 내용을 읽음
- 수정된 mmap page는 munmap 때 파일에 다시 반영될 수 있음
```

## 7. VM_FILE page란 무엇인가?

`VM_FILE`은 file-backed page를 의미한다.

즉 page의 내용이 파일과 연결되어 있다는 뜻이다.

```text
VM_ANON
- stack, heap, 실행 파일 segment처럼 일반 메모리 성격
- 파일과 계속 연결되어 있지 않음

VM_FILE
- mmap된 파일 page
- 파일과 연결되어 있음
- dirty면 write-back 대상
```

`VM_FILE` page는 보통 `struct file_page` 안에 파일 관련 정보를 가진다.

예상 필드는 다음과 같다.

```c
struct file_page {
	struct file *file;
	off_t ofs;
	size_t read_bytes;
	size_t zero_bytes;
};
```

이 필드들은 팀 구현 방식에 맞춰 조정할 수 있다.

핵심은 다음 정보가 필요하다는 것이다.

```text
어느 파일에서 읽을 것인가?
파일의 어느 offset부터 읽을 것인가?
몇 바이트를 파일에서 읽을 것인가?
나머지 몇 바이트를 0으로 채울 것인가?
```

## 8. aux는 왜 필요한가?

`vm_alloc_page_with_initializer()`는 page를 당장 frame에 올리지 않고 SPT에 예약 등록한다.

그런데 나중에 page fault가 났을 때 파일 내용을 읽으려면 정보가 필요하다.

```text
file
offset
read_bytes
zero_bytes
writable
```

이 정보를 보관하는 것이 `aux`다.

흐름은 다음과 같다.

```text
do_mmap()
-> page별 aux 생성
-> vm_alloc_page_with_initializer(VM_FILE, upage, writable, init, aux)
-> uninit_new()
-> page->uninit.aux = aux 저장

첫 접근
-> uninit_initialize()
-> init(page, aux)
-> file_page에 필요한 정보 저장 또는 frame에 내용 로딩
```

중요한 점은 `aux`는 보통 `malloc`으로 만든 보조 정보라는 것이다.

사용이 끝나면 적절한 시점에 `free(aux)`해야 한다.

## 9. AB팀 상세: syscall 연결

유저 라이브러리에는 이미 mmap/munmap syscall 래퍼가 있다.

```text
pintos/lib/user/syscall.c
```

```c
void *
mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
	return (void *) syscall5 (SYS_MMAP, addr, length, writable, fd, offset);
}

void
munmap (void *addr) {
	syscall1 (SYS_MUNMAP, addr);
}
```

하지만 커널의 `syscall_handler()`에는 `SYS_MMAP`, `SYS_MUNMAP` 분기가 필요하다.

AB팀은 보통 다음 흐름을 연결한다.

```text
SYS_MMAP
-> fd로 struct file * 찾기
-> do_mmap(addr, length, writable, file, offset)
-> 반환값을 f->R.rax에 저장

SYS_MUNMAP
-> do_munmap(addr)
```

주의할 점:

```text
fd 0, fd 1은 mmap 불가
잘못된 fd면 MAP_FAILED 반환
file이 NULL이면 MAP_FAILED 반환
```

보통 실패 반환은 다음 값을 쓴다.

```c
#define MAP_FAILED ((void *) -1)
```

이미 유저 헤더에 정의되어 있을 수 있으므로 확인해야 한다.

## 10. AB팀 상세: mmap validation

`do_mmap()`에서 가장 먼저 할 일은 요청 검증이다.

검증 조건은 테스트가 많이 본다.

### 10.1 addr이 NULL이면 실패

```text
mmap(NULL, ...)
-> 실패
```

이유:

```text
0번 주소는 잘못된 유저 주소로 취급
```

관련 테스트:

```text
mmap-null
```

### 10.2 addr이 page aligned가 아니면 실패

`mmap` 주소는 page 단위로 매핑되어야 한다.

따라서 `addr`의 page offset이 0이어야 한다.

```text
pg_ofs(addr) == 0
```

관련 테스트:

```text
mmap-misalign
```

### 10.3 length가 0이면 실패

매핑할 길이가 0이면 의미가 없다.

```text
length == 0
-> MAP_FAILED
```

관련 테스트:

```text
mmap-zero-len
```

### 10.4 offset이 page aligned가 아니면 실패

파일 offset도 page 단위로 시작해야 한다.

```text
offset % PGSIZE == 0
```

관련 테스트:

```text
mmap-bad-off
```

### 10.5 fd가 유효하지 않으면 실패

fd로 file을 찾을 수 있어야 한다.

```text
find_file_by_fd(fd) == NULL
-> 실패
```

fd 0과 fd 1도 mmap하면 안 된다.

```text
fd 0: stdin
fd 1: stdout
```

관련 테스트:

```text
mmap-bad-fd
mmap-bad-fd2
mmap-bad-fd3
```

### 10.6 파일 길이가 0이면 실패

빈 파일을 mmap하는 경우는 실패로 처리하는 것이 일반적이다.

```text
file_length(file) == 0
-> MAP_FAILED
```

관련 테스트:

```text
mmap-zero
```

### 10.7 매핑 범위가 유저 주소여야 한다

매핑하려는 전체 범위가 유저 주소 공간 안에 있어야 한다.

```text
addr
addr + length
```

둘 다 조심해야 한다.

특히 `addr + length` 계산에서 overflow가 날 수 있다.

관련 테스트:

```text
mmap-kernel
```

### 10.8 기존 page와 겹치면 실패

mmap하려는 범위에 이미 SPT page가 있으면 안 된다.

```text
for each page in [addr, addr + length)
    if spt_find_page(spt, page_addr) != NULL
        fail
```

관련 테스트:

```text
mmap-overlap
mmap-over-code
mmap-over-data
mmap-over-stk
```

즉 mmap은 기존 code/data/stack page 위에 덮어쓰면 안 된다.

## 11. AB팀 상세: mmap page registration

검증이 끝나면 파일 구간을 page 단위로 나누어 SPT에 등록한다.

핵심 계산은 `load_segment()`와 비슷하다.

```text
남은 length가 있다
-> 이번 page에서 파일에서 읽을 바이트 계산
-> 나머지는 0으로 채울 바이트 계산
-> aux 생성
-> VM_FILE page 등록
-> 다음 page로 이동
```

각 page마다 필요한 값:

```text
upage
file
offset
page_read_bytes
page_zero_bytes
writable
```

예시 흐름:

```text
remaining = length
current_addr = addr
current_offset = offset

while remaining > 0:
    page_read_bytes = min(remaining, PGSIZE)
    page_zero_bytes = PGSIZE - page_read_bytes

    aux = mmap_aux(file, current_offset, page_read_bytes, page_zero_bytes)
    vm_alloc_page_with_initializer(VM_FILE, current_addr, writable, lazy/init 함수, aux)

    remaining -= page_read_bytes
    current_addr += PGSIZE
    current_offset += page_read_bytes
```

주의:

```text
파일 길이보다 length가 더 길면?
-> 파일에서 읽을 수 있는 부분만 read_bytes
-> 나머지는 zero_bytes
```

즉 단순히 `remaining`만 보면 안 되고, 파일에서 실제로 읽을 수 있는 남은 크기도 고려해야 한다.

## 12. file_reopen이 왜 필요한가?

mmap page는 원본 file 객체와 연결되어야 한다.

하지만 유저가 mmap 후 fd를 close할 수 있다.

예:

```c
map = mmap(addr, length, writable, fd, offset);
close(fd);
// 이후에도 map으로 파일 내용을 읽을 수 있어야 함
```

이런 경우 fd table의 file을 그대로 쓰면 close 후 문제가 생길 수 있다.

그래서 mmap용으로 `file_reopen(file)`을 해서 별도의 file 객체를 들고 있는 방식이 안정적이다.

관련 테스트:

```text
mmap-close
```

정리:

```text
fd table file
-> close(fd) 때 닫힐 수 있음

mmap file
-> mmap page가 살아 있는 동안 필요
-> file_reopen으로 별도 참조를 갖는 것이 안전
```

## 13. CD팀 상세: file_backed_initializer()

`file_backed_initializer()`는 page를 실제 `VM_FILE` page처럼 동작하게 만든다.

위치:

```text
pintos/vm/file.c
```

핵심은 다음이다.

```c
page->operations = &file_ops;
```

이 한 줄이 실행되면 이후 이 page는 file-backed page처럼 동작한다.

```text
swap_in(page, kva)
-> file_backed_swap_in(page, kva)

swap_out(page)
-> file_backed_swap_out(page)

destroy(page)
-> file_backed_destroy(page)
```

추가로 `struct file_page` 안에 필요한 정보를 옮겨 담아야 한다.

예:

```text
file
ofs
read_bytes
zero_bytes
```

구현 방식은 두 가지가 가능하다.

```text
방식 1
-> initializer에서 aux 정보를 file_page로 옮김
-> swap_in은 page->file 정보만 사용

방식 2
-> lazy_load 함수에서 aux로 바로 file_read_at 수행
-> file_page에도 write-back용 정보를 저장
```

팀에서 하나로 통일해야 한다.

## 14. CD팀 상세: file_backed_swap_in()

`file_backed_swap_in()`은 file-backed page가 처음 frame에 올라올 때 실제 파일 내용을 읽는 함수다.

위치:

```text
pintos/vm/file.c
```

해야 할 일:

```text
1. page->file에서 file_page 정보 확인
2. file_read_at(file, kva, read_bytes, ofs)
3. 읽은 바이트가 read_bytes와 같은지 확인
4. kva + read_bytes부터 zero_bytes만큼 memset 0
5. 성공하면 true, 실패하면 false
```

핵심 함수:

```c
file_read_at(file, kva, read_bytes, ofs)
```

의미:

```text
file의 ofs 위치부터 read_bytes만큼 읽어서
frame의 kva 주소에 저장한다.
```

zero fill이 필요한 이유:

```text
page는 항상 4KB 단위
파일 내용이 4KB보다 작을 수 있음
남은 영역은 0으로 채워야 함
```

관련 테스트:

```text
mmap-read
mmap-remove
lazy-file
```

## 15. dirty bit란 무엇인가?

dirty bit는 page 내용이 수정되었는지 알려주는 표시다.

CPU/MMU가 page table entry에 관리한다.

Pintos에서는 다음 함수를 사용할 수 있다.

```c
pml4_is_dirty(thread_current()->pml4, page->va)
```

의미:

```text
이 page가 메모리에 올라온 뒤 수정되었는가?
```

수정되지 않았다면 파일에 다시 쓸 필요가 없다.

수정되었다면 `munmap` 때 파일에 write-back해야 한다.

```text
dirty == false
-> 파일에 쓰지 않아도 됨

dirty == true
-> file_write_at() 필요
```

관련 테스트:

```text
mmap-clean
mmap-write
```

## 16. CD팀 상세: file_backed_swap_out()

`file_backed_swap_out()`은 file-backed page를 frame에서 내보낼 때 처리하는 함수다.

Project 3 후반 eviction까지 생각하면 중요하다.

해야 할 일:

```text
1. page가 dirty인지 확인
2. dirty면 file_write_at()으로 파일에 반영
3. page table mapping 제거
4. page->frame 연결 정리
5. 성공 여부 반환
```

핵심 함수:

```c
file_write_at(file, kva, read_bytes, ofs)
```

write-back 할 때 보통 `read_bytes`만큼만 쓴다.

왜 4KB 전체를 쓰면 안 되는가?

```text
파일 마지막 page는 실제 파일 내용보다 page가 더 클 수 있음
zero_bytes 부분은 파일에 쓰면 파일 크기나 내용이 잘못될 수 있음
```

따라서 파일에 반영할 크기는 보통 다음이다.

```text
read_bytes
```

## 17. CD팀 상세: do_munmap()

`do_munmap(addr)`는 mmap 연결을 해제하는 함수다.

유저가 호출하는 syscall:

```c
munmap(addr);
```

커널 내부:

```text
syscall_handler
-> do_munmap(addr)
```

해야 할 일:

```text
1. addr에 해당하는 mmap 영역을 찾는다.
2. 그 영역에 속한 page들을 순회한다.
3. 각 page가 dirty면 파일에 write-back한다.
4. SPT에서 page를 제거한다.
5. frame/page 자원을 정리한다.
```

문제는 이것이다.

```text
addr 하나만 받고 mmap 영역 전체를 어떻게 아는가?
```

이를 해결하려면 mmap 영역 정보를 추적해야 한다.

## 18. mmap 영역 추적이 필요한 이유

`do_munmap(addr)`는 시작 주소만 받는다.

하지만 munmap은 그 mmap 호출로 만들어진 전체 영역을 해제해야 한다.

예:

```c
map = mmap(0x10000000, 8192, 1, fd, 0);
munmap(map);
```

이 경우 해제해야 하는 page는 2개다.

```text
0x10000000
0x10001000
```

따라서 커널은 mmap 호출마다 다음 정보를 기억해야 한다.

```text
map 시작 주소
map 길이
page 개수
file 정보
```

구현 방식은 여러 가지다.

```text
방식 1
-> thread 구조체에 mmap list 추가
-> mmap_region 구조체로 영역 추적

방식 2
-> 각 file_page에 map_start, page_count 같은 정보를 넣음
-> do_munmap에서 시작 page를 찾고 page_count만큼 순회

방식 3
-> 시작 주소부터 연속된 VM_FILE page를 찾으며 순회
-> 간단하지만 다른 mmap과 맞닿은 경우 위험할 수 있음
```

추천은 방식 1이다.

```text
thread가 mmap_region list를 가진다.
do_mmap 성공 시 region을 list에 추가한다.
do_munmap(addr)은 list에서 시작 주소가 addr인 region을 찾는다.
```

다만 팀 구현량을 줄이려면 방식 2 또는 3을 선택할 수도 있다.

중요한 것은 AB팀과 CD팀이 같은 방식으로 맞추는 것이다.

## 19. file_backed_destroy()

`file_backed_destroy()`는 file-backed page가 제거될 때 내부 자원을 정리하는 함수다.

일반적으로 다음을 고려한다.

```text
dirty면 write-back이 필요한가?
file_page가 들고 있는 file을 닫아야 하는가?
aux나 mmap metadata를 free해야 하는가?
frame 연결을 끊어야 하는가?
```

다만 write-back을 어디서 할지는 팀 기준을 정해야 한다.

```text
방식 1
-> do_munmap에서 dirty write-back 후 spt_remove_page()
-> destroy는 file close/free 중심

방식 2
-> file_backed_destroy() 안에서 dirty write-back까지 처리
-> do_munmap은 spt_remove_page()만 호출
```

초보자 입장에서는 방식 1이 흐름을 이해하기 쉽다.

```text
do_munmap
-> write-back 판단
-> spt_remove_page
```

하지만 중복 write-back을 막기 위해 책임을 명확히 해야 한다.

## 20. AB/CD가 반드시 먼저 합의해야 하는 것

AB팀과 CD팀은 구현 전에 아래를 맞춰야 한다.

### 20.1 aux 구조체

최소 필드:

```c
struct mmap_aux {
	struct file *file;
	off_t ofs;
	size_t read_bytes;
	size_t zero_bytes;
};
```

추가 가능:

```text
writable
map_start
map_length
page_count
```

### 20.2 struct file_page 필드

최소 필드:

```c
struct file_page {
	struct file *file;
	off_t ofs;
	size_t read_bytes;
	size_t zero_bytes;
};
```

추가 가능:

```text
map_start
map_length
list_elem
```

### 20.3 mmap 영역 추적 방식

선택지:

```text
thread mmap list
file_page에 region 정보 저장
연속 page 순회
```

추천:

```text
thread mmap list
```

### 20.4 write-back 책임 위치

선택지:

```text
do_munmap에서 write-back
file_backed_destroy에서 write-back
file_backed_swap_out에서 write-back
```

추천:

```text
do_munmap과 file_backed_swap_out의 책임을 나누되,
같은 page를 두 번 write-back하지 않도록 규칙을 정한다.
```

## 21. 구현 순서 추천

학습 순서와 구현 순서는 조금 다를 수 있다.

학습 순서:

```text
1. C - file-backed swap in
2. A - mmap validation
3. B - mmap page registration
4. D - munmap + write-back
```

구현 순서:

```text
1. 공통 구조 합의
   - mmap_aux
   - struct file_page
   - mmap region 추적 방식

2. CD팀이 file_backed_initializer / swap_in 기본 구현
   - VM_FILE page가 frame에 파일 내용을 읽을 수 있게 함

3. AB팀이 syscall SYS_MMAP 연결과 do_mmap validation 구현
   - 실패 조건 정리

4. AB팀이 do_mmap page registration 구현
   - VM_FILE page를 SPT에 등록

5. CD팀이 do_munmap / write-back 구현
   - dirty page 파일 반영
   - page 제거

6. 테스트 실행
```

## 22. 관련 테스트 묶음

### 22.1 validation 테스트

AB팀이 먼저 볼 테스트:

```text
mmap-null
mmap-misalign
mmap-zero-len
mmap-bad-fd
mmap-bad-fd2
mmap-bad-fd3
mmap-bad-off
mmap-kernel
mmap-overlap
mmap-over-code
mmap-over-data
mmap-over-stk
```

### 22.2 read/lazy loading 테스트

CD팀 swap_in과 AB팀 등록이 같이 맞아야 통과:

```text
lazy-file
mmap-read
mmap-remove
mmap-close
```

### 22.3 write-back 테스트

CD팀 munmap/write-back이 중요:

```text
mmap-write
mmap-clean
mmap-unmap
mmap-off
```

### 22.4 복합 테스트

전체 안정화 후:

```text
mmap-twice
mmap-shuffle
mmap-inherit
mmap-exit
swap-file
swap-iter
```

## 23. 개별 테스트 실행 예시

도커 bash 기준:

```bash
cd /workspaces/week11/pintos/vm/build

mkdir -p /tmp/pintos-utils
cp /workspaces/week11/pintos/utils/pintos /tmp/pintos-utils/pintos
sed -i 's/\r$//' /tmp/pintos-utils/pintos
chmod +x /tmp/pintos-utils/pintos
export PATH=/tmp/pintos-utils:/workspaces/week11/pintos/utils:$PATH
```

예시:

```bash
rm -f tests/vm/mmap-read.output tests/vm/mmap-read.errors tests/vm/mmap-read.result
make tests/vm/mmap-read.result
cat tests/vm/mmap-read.result
cat tests/vm/mmap-read.errors
```

validation 예시:

```bash
rm -f tests/vm/mmap-null.output tests/vm/mmap-null.errors tests/vm/mmap-null.result
make tests/vm/mmap-null.result
cat tests/vm/mmap-null.result
```

## 24. 자주 하는 실수

### 24.1 mmap 순간 파일 내용을 바로 다 읽어버림

Project 3에서는 lazy loading 흐름을 써야 한다.

```text
mmap 때는 SPT 등록
첫 접근 때 file_backed_swap_in
```

### 24.2 aux를 free하지 않음

`aux`를 malloc했다면 사용 후 free해야 한다.

다만 너무 빨리 free하면 lazy loading 때 정보를 못 쓴다.

```text
SPT 등록 직후 free하면 안 됨
lazy loading에서 사용한 뒤 free 또는 file_page로 옮긴 뒤 free
```

### 24.3 fd close 후 mmap이 깨짐

`file_reopen()` 없이 fd table file만 들고 있으면 `close(fd)` 후 mmap page가 파일을 못 읽을 수 있다.

### 24.4 overlap 검사 누락

기존 code/data/stack page 위에 mmap을 허용하면 큰 문제가 생긴다.

SPT로 page 단위 overlap을 반드시 확인해야 한다.

### 24.5 dirty가 아닌 page도 write-back

`mmap-clean`은 clean page를 write-back하면 실패할 수 있다.

dirty bit 확인이 중요하다.

### 24.6 파일 마지막 page에서 4KB 전체 write-back

파일 마지막 page는 `read_bytes`만큼만 파일에 해당한다.

zero 영역까지 쓰면 파일 내용이 잘못될 수 있다.

### 24.7 mmap 영역 추적 없이 munmap 구현

`munmap(addr)`는 시작 주소 하나만 받는다.

전체 mmap 영역을 알 수 있는 구조가 없으면 여러 page를 제대로 해제하기 어렵다.

## 25. AB팀 한 문장 요약

AB팀은 **mmap 요청이 유효한지 검사하고, 파일 구간을 page 단위로 나누어 VM_FILE page들을 SPT에 lazy loading 형태로 등록하는 역할**이다.

## 26. CD팀 한 문장 요약

CD팀은 **SPT에 등록된 VM_FILE page가 처음 접근될 때 파일 내용을 frame에 읽고, munmap 때 dirty page를 파일에 write-back한 뒤 page를 제거하는 역할**이다.

## 27. Merge 3 전체 한 문장 요약

Merge 3는 **파일을 유저 가상 메모리에 lazy하게 연결하고, 접근 시 파일 내용을 frame에 읽어오며, 해제 시 수정된 내용을 파일에 반영하는 mmap/munmap 흐름을 완성하는 단계**이다.

