# Pintos Project 3 VM - Stack Growth 완전 정리

## 1. 이 문서의 목표

이 문서는 Merge 2에서 구현할 `stack growth`를 깊게 이해하기 위한 정리 문서다.

목표는 다음 질문에 답하는 것이다.

- stack growth가 왜 필요한가?
- page fault와 stack growth는 어떤 관계인가?
- 어떤 page fault를 stack growth로 인정해야 하는가?
- `rsp`, `fault_addr`, `USER_STACK`, stack limit은 각각 무엇인가?
- `vm_try_handle_fault()`와 `vm_stack_growth()`는 각각 무엇을 해야 하는가?
- syscall 중 발생하는 stack growth는 왜 따로 조심해야 하는가?
- 어떤 테스트가 이 기능을 검증하는가?

## 2. Stack Growth 한 문장 요약

Stack growth는 **스택이 아래 방향으로 커지다가 아직 매핑되지 않은 주소에 접근했을 때, 커널이 합법적인 스택 접근인지 확인한 뒤 새 anonymous page를 만들어주는 기능**이다.

```text
스택 아래쪽 주소 접근
-> page fault 발생
-> 합법적인 stack access인지 검사
-> 맞으면 새 VM_ANON page 생성
-> frame 연결
-> 유저 프로그램 계속 실행
```

## 3. 왜 Stack Growth가 필요한가?

Project 2에서는 유저 스택을 처음부터 한 페이지 정도만 만들었다.

```text
낮은 주소                                      높은 주소
USER_STACK - PGSIZE ------------------------ USER_STACK
                 초기 스택 1페이지
```

처음에는 이 정도면 argument passing과 간단한 함수 실행이 가능하다.

하지만 프로그램이 실행되면서 다음 일이 생길 수 있다.

```text
함수 호출이 많아짐
지역 변수가 커짐
배열을 스택에 크게 잡음
syscall이 유저 버퍼에 값을 써야 함
```

스택은 높은 주소에서 낮은 주소로 자란다.

```text
USER_STACK
   ↓ push, 함수 호출, 지역 변수
   ↓
   ↓
낮은 주소 방향으로 성장
```

그러다가 기존 스택 페이지보다 아래 주소에 접근하면 아직 그 주소에는 page table 매핑이 없다.

그러면 CPU는 page fault를 발생시킨다.

이때 그 접근이 정상적인 스택 확장이라면 프로세스를 죽이면 안 된다.

커널이 새 스택 page를 만들어서 프로그램을 계속 실행하게 해야 한다.

## 4. 전체 흐름

```text
유저 프로그램 실행 중
-> 스택 아래쪽 주소 접근
-> page table에 매핑 없음
-> page fault 발생
-> exception.c의 page_fault()
-> fault_addr = rcr2()
-> not_present/write/user 계산
-> vm_try_handle_fault(f, fault_addr, user, write, not_present)
-> SPT에서 기존 page 찾기
   -> 있으면 기존 lazy/swapped page claim
   -> 없으면 stack growth 후보인지 검사
-> 후보가 맞으면 vm_stack_growth(fault_addr)
-> 새 stack page를 SPT에 등록
-> vm_claim_page()로 frame 연결
-> page fault handler가 return
-> CPU가 실패했던 명령을 다시 실행
-> 이번에는 매핑이 있으므로 성공
```

핵심은 마지막 부분이다.

page fault handler가 성공적으로 return하면 유저 프로그램이 다음 명령으로 넘어가는 것이 아니라, **fault가 났던 명령을 다시 시도한다.**

그래서 커널이 그 사이에 page를 만들어두면, 같은 명령이 이번에는 성공한다.

## 5. 중요한 주소들

### 5.1 USER_STACK

`USER_STACK`은 유저 스택의 최상단 주소다.

현재 코드에서는 다음 파일에 정의되어 있다.

```text
pintos/include/threads/vaddr.h
```

```c
#define USER_STACK 0x47480000
```

스택은 여기서 시작해서 아래로 자란다.

```text
낮은 주소                                      높은 주소
... ------------------------------- USER_STACK
                         스택 시작점
```

### 5.2 rsp

`rsp`는 현재 스택 포인터다.

64비트 x86-64에서 스택 위치를 나타내는 레지스터다.

```text
rsp
-> 지금 스택의 현재 위치
-> push하면 rsp가 감소
-> pop하면 rsp가 증가
```

초기 스택 설정 후에는 보통 다음 상태다.

```text
rsp = USER_STACK
```

argument passing 후에는 `rsp`가 더 아래로 내려간다.

### 5.3 fault_addr

`fault_addr`는 page fault를 일으킨 실제 가상 주소다.

`exception.c`의 `page_fault()`에서 다음처럼 얻는다.

```c
fault_addr = (void *) rcr2();
```

중요한 점은 `fault_addr`와 `rsp`가 같지 않을 수 있다는 것이다.

```text
rsp
-> 현재 스택 포인터

fault_addr
-> 실제로 접근하려다가 실패한 주소
```

stack growth 판단은 `rsp` 자체보다 **fault_addr가 rsp 근처인지**를 본다.

## 6. Page Fault 정보

`page_fault()`는 fault 원인을 세 가지 값으로 해석한다.

```c
not_present = (f->error_code & PF_P) == 0;
write = (f->error_code & PF_W) != 0;
user = (f->error_code & PF_U) != 0;
```

각 의미는 다음과 같다.

```text
not_present
-> page table에 매핑이 없어서 난 fault인가?
-> true면 새 page를 만들거나 기존 page를 claim할 가능성이 있다.

write
-> 쓰기 접근 중 발생한 fault인가?
-> read fault일 수도 있으므로 stack growth에서 무조건 write만 허용할지는 신중해야 한다.

user
-> 유저 모드에서 발생한 fault인가?
-> false면 커널 모드에서 유저 주소 접근 중 fault가 났을 수 있다.
```

stack growth는 기본적으로 `not_present == true`인 경우에만 생각한다.

```text
not_present == true
-> 없는 page 접근
-> lazy loading 또는 stack growth 가능

not_present == false
-> 이미 매핑된 page에 권한 위반
-> stack growth로 처리하면 안 됨
```

## 7. Stack Growth로 인정할 조건

모든 page fault를 stack growth로 처리하면 안 된다.

아무 주소나 허용하면 잘못된 포인터 접근도 스택으로 인정해버린다.

대표 조건은 다음과 같다.

```text
1. addr이 NULL이 아니어야 한다.
2. addr이 유저 가상 주소여야 한다.
3. not_present fault여야 한다.
4. addr이 USER_STACK보다 아래여야 한다.
5. addr이 최대 스택 크기 제한 안에 있어야 한다.
6. addr이 현재 user rsp 근처여야 한다.
7. SPT에 기존 page가 없을 때만 stack growth 후보로 본다.
```

### 7.1 addr이 유저 주소여야 한다

커널 주소를 유저 스택으로 늘리면 안 된다.

```text
is_user_vaddr(addr) == true
```

또는

```text
!is_kernel_vaddr(addr)
```

이어야 한다.

### 7.2 USER_STACK 아래여야 한다

스택은 `USER_STACK`에서 아래로 자란다.

따라서 fault 주소가 `USER_STACK`보다 위라면 스택이 아니다.

```text
addr < USER_STACK
```

### 7.3 최대 스택 크기 제한

Pintos Project 3에서는 stack 크기를 최대 1MB로 제한해야 한다.

```text
STACK_LIMIT = 1MB
stack_bottom_limit = USER_STACK - 1MB
```

따라서 stack growth 후보 주소는 다음 범위 안에 있어야 한다.

```text
USER_STACK - 1MB <= addr < USER_STACK
```

이 제한이 없으면 유저 프로그램이 아무 아래 주소나 접근해서 스택을 무한히 늘릴 수 있다.

### 7.4 rsp 근처인지 확인해야 한다

가장 중요한 조건이다.

정상적인 스택 접근이라면 fault 주소가 현재 user `rsp` 근처여야 한다.

예를 들어 `push` 명령은 `rsp`를 감소시키고 그 위치에 값을 저장한다.

x86-64에서는 8바이트 단위 push가 흔하므로, fault 주소가 `rsp - 8` 근처에서 날 수 있다.

```text
addr >= rsp - 8
```

팀 정책에 따라 여유를 더 크게 잡을 수도 있다.

예를 들어 과거 Pintos 문서나 일부 설명에서는 32바이트 여유를 말한다.

```text
addr >= rsp - 32
```

둘 중 하나를 팀 기준으로 정해야 한다.

초보자 기준 추천은 다음과 같다.

```text
정확한 x86-64 push 기준을 우선하면 8바이트
테스트와 문서 호환을 넉넉히 보려면 32바이트
```

중요한 것은 "rsp보다 너무 멀리 아래인 주소"는 stack growth로 인정하면 안 된다는 점이다.

예를 들어 아래 테스트는 실패해야 한다.

```c
asm volatile ("movq -4096(%rsp), %rax");
```

이 접근은 `rsp`보다 한 페이지나 아래를 읽는다.

정상적인 push나 함수 호출이 아니라 너무 멀리 떨어진 접근이므로 stack growth로 처리하면 안 된다.

## 8. SPT hit와 SPT miss를 먼저 구분해야 한다

`vm_try_handle_fault()`에서 가장 먼저 구분해야 하는 것은 이것이다.

```text
fault_addr에 해당하는 page가 SPT에 이미 있는가?
```

### 8.1 SPT hit

SPT에서 page를 찾았다면 stack growth가 아니다.

이미 등록된 lazy page 또는 swapped page일 가능성이 있다.

이 경우는 기존 claim 흐름으로 처리한다.

```text
spt_find_page(spt, addr) 성공
-> writable 검사
-> vm_do_claim_page(page)
```

### 8.2 SPT miss

SPT에서 page를 못 찾았다면 두 가능성이 있다.

```text
1. 잘못된 주소 접근
2. 아직 만들어지지 않은 stack page 접근
```

이때만 stack growth 조건을 검사한다.

```text
spt_find_page(spt, addr) 실패
-> stack growth 조건 검사
-> 맞으면 vm_stack_growth(addr)
-> 아니면 false
```

즉 모든 SPT miss가 stack growth는 아니다.

## 9. vm_try_handle_fault()의 역할

`vm_try_handle_fault()`는 page fault를 복구할 수 있는지 판단하는 입구 함수다.

위치:

```text
pintos/vm/vm.c
```

역할:

```text
1. fault 주소가 유효한지 검사
2. not_present fault인지 검사
3. SPT에서 기존 page를 찾음
4. 기존 page가 있으면 claim
5. 기존 page가 없으면 stack growth 후보인지 검사
6. 후보가 맞으면 vm_stack_growth() 호출
7. 처리 가능하면 true, 불가능하면 false 반환
```

흐름:

```text
vm_try_handle_fault(f, addr, user, write, not_present)
-> addr 기본 검증
-> not_present 확인
-> page = spt_find_page(spt, addr)
   -> page 있음: 권한 검사 후 vm_do_claim_page(page)
   -> page 없음: stack growth 조건 검사
        -> 가능: vm_stack_growth(addr)
        -> 불가: false
```

주의할 점:

```text
page == NULL인 상태로 vm_do_claim_page(page)를 호출하면 안 된다.
```

이 실수는 바로 커널 패닉이나 테스트 무출력 실패로 이어질 수 있다.

## 10. vm_stack_growth()의 역할

`vm_stack_growth()`는 실제로 새 stack page를 만드는 함수다.

위치:

```text
pintos/vm/vm.c
```

템플릿은 보통 다음 형태다.

```c
static void
vm_stack_growth (void *addr UNUSED) {
}
```

역할:

```text
1. fault addr를 page 시작 주소로 내림
2. 그 주소를 VM_ANON page로 SPT에 등록
3. 바로 claim해서 frame과 page table에 연결
```

핵심 계산:

```c
void *va = pg_round_down(addr);
```

왜 `addr - PGSIZE`가 아니라 `pg_round_down(addr)`인가?

fault가 꼭 page의 맨 아래나 기존 stack_bottom 바로 아래에서 발생하는 것이 아니기 때문이다.

CPU가 접근한 실제 주소가 page 중간일 수 있으므로, 그 주소가 속한 page의 시작 주소를 구해야 한다.

```text
fault addr = 0x4747eabc
pg_round_down(addr) = 0x4747e000
```

새 page의 va는 `0x4747e000`이 된다.

## 11. vm_stack_growth()는 void로 둘까 bool로 바꿀까?

템플릿은 `void`다.

하지만 학습과 디버깅 관점에서는 `bool`이 더 이해하기 쉽다.

### void 유지

장점:

```text
템플릿과 문서 시그니처를 그대로 따른다.
```

단점:

```text
성공/실패를 vm_try_handle_fault()에서 바로 알기 어렵다.
실패 처리를 간접적으로 해야 한다.
```

### bool로 변경

장점:

```text
성공하면 true, 실패하면 false라 흐름이 명확하다.
디버깅이 쉽다.
```

단점:

```text
static 함수라 부담은 적지만, 템플릿 시그니처를 바꾸는 결정이 필요하다.
팀원 코드와 합의해야 한다.
```

페어 구현에서는 둘 중 하나를 먼저 합의해야 한다.

초보자 관점 추천:

```text
팀 합의가 가능하면 bool vm_stack_growth(void *addr)로 바꾸는 편이 이해하기 쉽다.
문서 시그니처를 엄격히 따르려면 void 유지도 가능하다.
```

## 12. user rsp 문제

stack growth에서 가장 헷갈리는 부분이다.

`page_fault()`에 전달되는 `struct intr_frame *f`에는 `rsp`가 있다.

유저 모드에서 page fault가 발생했다면 보통 `f->rsp`를 user stack pointer로 볼 수 있다.

하지만 커널 모드에서 유저 메모리에 접근하다가 page fault가 발생한 경우는 조심해야 한다.

예:

```text
sys_read(fd, user_buffer, size)
-> 커널이 user_buffer에 데이터를 복사
-> user_buffer가 아직 stack growth가 필요한 주소
-> 커널 모드에서 page fault 발생
```

이때 `f->rsp`는 유저의 `rsp`가 아닐 수 있다.

왜냐하면 CPU는 유저 모드에서 커널 모드로 처음 넘어올 때만 유저 `rsp`를 저장하고, 이미 커널 모드에서 발생한 fault에서는 커널 쪽 상태가 섞일 수 있기 때문이다.

그래서 더 안정적인 구현은 다음과 같은 user rsp 저장 전략을 쓴다.

```text
syscall_handler 진입 시
-> f->rsp를 현재 thread에 저장

page_fault에서 user == true
-> f->rsp 사용 가능

page_fault에서 user == false
-> thread에 저장해둔 user rsp 사용
```

이를 위해 `struct thread`에 필드를 추가하는 설계를 할 수 있다.

예:

```c
void *rsp_stack;
```

또는 팀 이름 규칙에 맞춰:

```c
void *user_rsp;
```

중요한 것은 이름보다 역할이다.

```text
커널 모드 page fault에서도 user stack pointer 기준으로 stack growth를 판단할 수 있어야 한다.
```

## 13. syscall 중 stack growth가 필요한 이유

다음 테스트를 보면 이해가 쉽다.

```c
char buf2[65536];
read(handle, buf2 + 32768, slen);
```

`buf2`는 큰 지역 배열이라 스택에 잡힌다.

그런데 `read()` syscall 안에서 커널이 `buf2 + 32768` 주소에 파일 내용을 써야 한다.

이 주소가 아직 매핑되지 않은 stack page일 수 있다.

흐름:

```text
유저 프로그램이 read syscall 호출
-> 커널 syscall_handler 진입
-> sys_read가 user buffer에 데이터 복사
-> user buffer 주소가 아직 stack page로 없음
-> 커널 모드에서 page fault
-> stack growth로 처리해야 함
```

이 경우 `user == false`일 수 있으므로 `f->rsp`만 믿으면 안 된다.

그래서 syscall 진입 시 user rsp 저장이 필요할 수 있다.

## 14. 구현 순서 추천

페어로 구현한다면 다음 순서가 좋다.

```text
1. Merge 1 page fault 기본 복구가 되는지 확인
   - SPT hit 시 vm_do_claim_page(page)

2. stack growth 조건 함수 설계
   - addr 유효성
   - USER_STACK 범위
   - 1MB limit
   - rsp 근처 조건

3. vm_stack_growth 구현
   - pg_round_down(addr)
   - vm_alloc_page(VM_ANON, va, true)
   - vm_claim_page(va)

4. vm_try_handle_fault에 연결
   - SPT miss일 때만 stack growth 검사

5. syscall 중 user rsp 저장 필요 여부 확인
   - pt-grow-stk-sc 테스트 대응

6. 테스트 실행
```

## 15. 구현 흐름 의사코드

정확한 코드는 팀 구현 상태에 맞춰야 한다.

여기서는 흐름만 본다.

```text
vm_try_handle_fault(f, addr, user, write, not_present)
    if addr is invalid
        return false

    if not_present is false
        return false

    page = spt_find_page(spt, addr)

    if page exists
        if write access and page is not writable
            return false
        return vm_do_claim_page(page)

    user_rsp = decide_user_rsp(f, user)

    if is_valid_stack_growth(addr, user_rsp)
        return vm_stack_growth(addr)

    return false
```

```text
vm_stack_growth(addr)
    va = pg_round_down(addr)

    if va is outside stack limit
        fail

    if vm_alloc_page(VM_ANON, va, true) fails
        fail

    if vm_claim_page(va) fails
        fail

    success
```

## 16. stack growth 조건 예시

아래 조건은 개념 예시다.

팀 구현에서는 함수 이름과 필드 이름을 맞춰야 한다.

```text
addr != NULL
is_user_vaddr(addr)
addr < USER_STACK
addr >= USER_STACK - 1MB
addr >= user_rsp - 8 또는 user_rsp - 32
not_present == true
```

`write` 조건은 신중해야 한다.

스택은 보통 쓰기 접근으로 커지지만, 테스트 중에는 읽기 접근이 잘못된 스택 접근인지 확인하는 경우도 있다.

따라서 단순히 `write == true`만 허용하면 일부 정상 케이스를 놓칠 수 있고, 반대로 너무 넓게 허용하면 잘못된 접근을 살릴 수 있다.

더 중요한 기준은 다음이다.

```text
fault address가 user rsp 근처인가?
stack limit 안인가?
```

## 17. 성공 케이스와 실패 케이스

### 성공해야 하는 케이스

```text
초기 스택보다 조금 아래 주소에 정상 접근
큰 지역 배열을 사용
syscall에서 스택 위 user buffer에 읽기/쓰기
```

예:

```c
char stack_obj[4096];
memset(stack_obj, 0, sizeof stack_obj);
```

### 실패해야 하는 케이스

```text
NULL 접근
kernel address 접근
USER_STACK보다 위쪽 접근
1MB stack limit보다 아래 접근
rsp보다 너무 멀리 아래 접근
권한 위반 fault
```

예:

```c
asm volatile ("movq -4096(%rsp), %rax");
```

이 코드는 `rsp`보다 너무 멀리 떨어진 주소를 읽으므로 stack growth로 살리면 안 된다.

## 18. 테스트 관점

stack growth 관련 대표 테스트는 다음과 같다.

```text
tests/vm/pt-grow-stack
-> 스택이 정상적으로 커지는지 확인

tests/vm/pt-grow-bad
-> rsp에서 너무 멀리 떨어진 접근을 거부하는지 확인

tests/vm/pt-grow-stk-sc
-> syscall 중 user buffer가 stack growth를 필요로 할 때 처리되는지 확인
```

개별 테스트 실행 예시:

```bash
cd /workspaces/week11/pintos/vm
make
cd build
export PATH=/workspaces/week11/pintos/utils:$PATH
make tests/vm/pt-grow-stack.result
cat tests/vm/pt-grow-stack.result
```

추가:

```bash
make tests/vm/pt-grow-bad.result
cat tests/vm/pt-grow-bad.result

make tests/vm/pt-grow-stk-sc.result
cat tests/vm/pt-grow-stk-sc.result
```

## 19. 자주 하는 실수

### 19.1 SPT miss를 전부 stack growth로 처리

잘못된 주소 접근까지 살려버린다.

반드시 `rsp` 근처 조건과 stack limit을 확인해야 한다.

### 19.2 page == NULL로 vm_do_claim_page 호출

SPT에서 page를 못 찾았는데 바로 claim하면 터진다.

```text
SPT hit
-> vm_do_claim_page(page)

SPT miss
-> stack growth 조건 검사
```

둘을 분리해야 한다.

### 19.3 fault_addr를 그대로 page va로 사용

page는 4KB 단위로 관리한다.

따라서 새 page를 만들 때는 반드시 page 시작 주소로 내려야 한다.

```c
va = pg_round_down(addr);
```

### 19.4 f->rsp만 항상 믿기

커널 모드 page fault에서는 `f->rsp`가 user rsp가 아닐 수 있다.

syscall 중 stack growth를 처리하려면 user rsp 저장 전략이 필요할 수 있다.

### 19.5 stack limit 누락

1MB 제한을 안 두면 스택이 무한히 자랄 수 있다.

```text
USER_STACK - 1MB <= addr < USER_STACK
```

이 범위를 지켜야 한다.

## 20. 페어 작업 분담 추천

2명이 stack growth를 페어로 한다면 이렇게 나누는 것이 좋다.

### 1명: 판별 담당

담당:

```text
vm_try_handle_fault()
is_valid_stack_access() 같은 helper
user rsp 기준 정리
not_present / user address / stack limit 검사
```

주요 질문:

```text
이 fault를 stack growth로 살려도 되는가?
살리면 안 되는 fault는 무엇인가?
syscall 중 fault의 rsp는 어떻게 볼 것인가?
```

### 1명: 실행 담당

담당:

```text
vm_stack_growth()
pg_round_down(addr)
vm_alloc_page(VM_ANON, va, true)
vm_claim_page(va)
실패 시 반환 처리
```

주요 질문:

```text
새 stack page의 va는 무엇인가?
SPT 등록과 claim 순서는 어떻게 되는가?
실패하면 어떤 값이 위로 전달되어야 하는가?
```

### 둘이 같이 맞춰야 하는 부분

```text
vm_stack_growth()를 void로 유지할지 bool로 바꿀지
rsp 근처 허용 범위를 8바이트로 할지 32바이트로 할지
user rsp 저장 필드가 필요한지
stack limit 상수를 어디에 둘지
```

## 21. 머릿속 최종 그림

```text
page fault 발생
│
├─ 기존 SPT page가 있는가?
│  ├─ 예
│  │  └─ 기존 lazy page claim
│  │     └─ vm_do_claim_page()
│  │
│  └─ 아니오
│     └─ stack growth 후보인가?
│        ├─ 아니오
│        │  └─ false 반환 -> process exit 또는 kill
│        │
│        └─ 예
│           └─ vm_stack_growth(addr)
│              ├─ pg_round_down(addr)
│              ├─ VM_ANON page 등록
│              ├─ vm_claim_page()
│              └─ true 반환
│
└─ page_fault()가 return
   └─ CPU가 실패했던 명령 재시도
```

## 22. 한 문장 요약

Stack growth는 SPT에 없는 주소에서 page fault가 났을 때, 그 주소가 user `rsp` 근처이고 1MB stack limit 안에 있는 정상 스택 접근이라면 새 `VM_ANON` page를 만들고 claim해서 프로그램을 계속 실행하게 만드는 기능이다.

