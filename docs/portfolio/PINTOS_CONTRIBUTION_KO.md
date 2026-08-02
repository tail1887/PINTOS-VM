# Pintos 개인 기여와 검증 근거

이 문서는 포트폴리오의 Pintos 설명을 약 3분 안에 검토할 수 있도록 정리한 근거 안내서입니다.
성과를 넓혀 보이기보다 개인 구현, 팀 통합, AI 보조 범위와 확인 가능한 검증 수준을 구분합니다.
내용은 2026-08-02 기준 `tail1887/PINTOS-VM`의 `main`(`76606fa97dcdb7a00167dac2ed11921441f03d6b`)과 연결된 주차별 저장소의 공개 기록을 대조해 작성했습니다.

## 1. 30초 요약

- 두 팀에서 Pintos를 단계적으로 수행했습니다.
  - 1차 팀: Thread·동기화와 User Program
  - 2차 팀: 기존 코드 이력을 이어받아 Virtual Memory
- 개인 기여의 중심은 다음 두 축입니다.
  - User Program: 인자 전달, 사용자 메모리 입력 검증, 프로세스·파일 자원 생명주기 통합
  - Virtual Memory: 지연 로딩 메타데이터, 페이지 유형별 정리, file-backed 복원, anon swap 상태 관리
- PR #123과 PR #183은 여러 사람의 변경이 포함된 팀 통합 단위입니다.
  개인 기여 근거로는 작성자·변경 코드가 확인되는 개별 커밋과 소규모 PR을 우선합니다.
- 일부 커밋은 Cursor와 공동 작성했습니다.
  공동 작성 표기를 숨기지 않으며, 개인 단독 작성과 같은 의미로 표현하지 않습니다.
- 저장소의 GitHub Actions 성공은 Pintos 전체 테스트 성공을 뜻하지 않습니다.
  공개 워크플로가 확인하는 것은 timer-sleep 소스 계약 2건뿐입니다.

## 2. 두 팀, 두 단계

| 단계 | 저장소·대표 범위 | 이 문서에서 다루는 개인 기여 |
| --- | --- | --- |
| 1차 팀 | PINTOS_WEEK9 → PINTOS-_WEEK10 | 스케줄링·동기화 정리, Argument Passing, syscall 입력 검증, 프로세스 자원 생명주기 |
| 2차 팀 | PINTOS-VM | lazy segment 메타데이터, 페이지 destroy, file-backed swap-in, anon swap과 실패 롤백 |

PINTOS-VM은 앞 단계의 코드와 커밋 이력을 이어받았습니다.
따라서 세 개의 주차별 저장소를 별도 프로젝트로 부풀리지 않고,
두 팀에서 하나의 운영체제 과제를 단계적으로 수행한 경험으로 설명합니다.

## 3. 기여 경계

| 구분 | 포함 기준 | 해석 방법 |
| --- | --- | --- |
| 개인 구현 | 박태정 작성 커밋이며 실제 변경 코드가 확인되는 범위 | 해당 함수와 실패 경로까지 코드로 확인 |
| 팀 통합 | 여러 팀원의 커밋을 한 PR에서 병합한 범위 | PR 전체를 개인 구현으로 표현하지 않음 |
| Cursor 공동 작업 | 커밋에 Cursor 공동 작성 표기가 있는 범위 | 공동 작성 표기를 공개하고 단독 작성으로 표현하지 않음 |
| 미완료·미검증 | 병합되지 않았거나 현재 코드에서 결함이 확인되는 범위 | 성과에서 제외하고 남은 한계로 명시 |

커밋 메타데이터만으로 Cursor가 생성한 코드의 정확한 비율은 재구성하지 않습니다. 공동 작성 표기가 있는 변경은 Cursor 보조 구현으로 분류하고, 저는 과제 범위와 적용 여부를 정해 diff를 저장소에 반영하고 실패 경로를 확인한 책임으로 설명합니다.

대표 근거 중 9d2ae31, 0886ffd와 이 문서에 연결한 VM 개인 커밋에는 Cursor 공동 작성 표기가 있습니다.
7f8ce81, ca7a0a1, 167ee23은 박태정 단독 작성자로 기록되어 있습니다.

## 4. User Program

### 4.1 Argument Passing과 사용자 진입 상태

개인 커밋에서 명령행을 토큰화하고 argc·argv를 구성한 뒤,
사용자 스택에 문자열, 정렬 패딩, NULL sentinel, argv 포인터,
fake return address를 ABI 순서에 맞게 배치했습니다.
rdi와 rsi에는 각각 argc와 argv를 전달했습니다.

큰 인자 배열이 제한된 커널 스택을 잠식하지 않도록 argv와 주소 배열을
별도의 page로 할당했고, 중간 실패 경로에서도 할당한 page를 해제하도록 정리했습니다.

대표 근거:

- [Argument Passing 완성 구현 커밋 9d2ae31](https://github.com/tail1887/PINTOS-VM/commit/9d2ae31c0bc93b0bfb6d7ff369d365375444121c)
- [인자 배열 page 할당 전환 커밋 7f8ce81](https://github.com/tail1887/PINTOS-VM/commit/7f8ce81ed5bbf3598da102adc82876ba52058dcc)

### 4.2 syscall 입력 검증

syscall 경계에서 사용자 포인터, 문자열, 버퍼의 시작·끝과 page 경계를
공통 helper로 검사하고 실패 시 공통 종료 경로로 연결했습니다.
검사는 NULL과 커널 주소 범위, 버퍼의 경계 접근을 방어하는 역할을 합니다.

다만 현재 helper 자체만으로 모든 unmapped address를 검출한다고 표현하지 않습니다.
페이지 매핑 여부까지 포괄하는 완전한 검증으로 확대 해석하면 안 됩니다.

대표 근거:

- [사용자 메모리 검증 helper 커밋 0886ffd](https://github.com/tail1887/PINTOS-VM/commit/0886ffd46d6d17c484e43da58994c41c5d174585)

### 4.3 프로세스와 파일 자원 생명주기

fork·wait·exit와 파일 관련 syscall을 통합하면서,
부모와 자식이 공유하는 child_status에 대기 semaphore와 참조 수를 두었습니다.
파일 디스크립터 복제 도중 실패하면 이미 복제한 파일을 닫는 롤백 경로도 다뤘습니다.

PR #123은 넓은 User Program 범위를 묶은 팀 통합 PR입니다.
따라서 PR 전체의 23개 파일과 모든 syscall을 단독 구현했다고 주장하지 않고,
위 개인 커밋과 실제 diff로 확인되는 범위만 개인 기여로 사용합니다.

대표 근거:

- [프로세스·syscall 통합 커밋 ca7a0a1](https://github.com/tail1887/PINTOS-VM/commit/ca7a0a1df48bb3e3d98e7fafc0f94833db3aa75a)
- [initd·wait 보완 커밋 167ee23](https://github.com/tail1887/PINTOS-VM/commit/167ee2300aa406e5209224abc6fd0c445eeb5503)
- [팀 통합 PR #123](https://github.com/seonho12-54/PINTOS-_WEEK10/pull/123)

## 5. Virtual Memory

### 5.1 실행 파일 지연 로딩 메타데이터

실행 파일 segment를 즉시 읽는 대신,
file·offset·read bytes·zero bytes 정보를 segment_aux에 담아
page fault 시 사용할 callback으로 전달하는 연결 경로를 구현했습니다.

이 기여는 lazy loading 전체 완성이 아니라 메타데이터와 callback 연결 범위입니다.
PR 리뷰에는 VM type 인자에 관한 미해결 질문도 남아 있어 그 이상으로 확대하지 않습니다.

- [lazy segment 연결 PR #163](https://github.com/tail1887/PINTOS-VM/pull/163)
- [개인 구현 커밋 e8d128b](https://github.com/tail1887/PINTOS-VM/commit/e8d128b30d878157645f0b68831364d858f28be5)

### 5.2 페이지 유형별 초기 정리

UNINIT, ANON, FILE page가 가진 초기 자원의 차이를 나누어
aux, page-table 매핑, frame, kernel page를 해제하는 destroy 경로를 구현했습니다.

이 커밋은 이후 팀이 보완한 swap slot, dirty write-back, file close까지 포함하는
최종 destroy 전체가 아니라 당시의 초기 정리 범위입니다.

- [page destroy PR #168](https://github.com/tail1887/PINTOS-VM/pull/168)
- [개인 구현 커밋 c641f04](https://github.com/tail1887/PINTOS-VM/commit/c641f045b77201b79caca59e76094e5c2559b644)

### 5.3 file-backed 메타데이터와 복원

UNINIT page가 file-backed page로 전환될 때 파일 위치와 읽기·zero-fill 정보를 옮기고,
frame 재확보 시 파일 내용을 읽은 뒤 남은 영역을 0으로 채우는 swap-in 경로를 구현했습니다.

PR #171의 테스트 계획 체크박스는 모두 미체크 상태입니다.
따라서 코드와 병합 사실은 근거로 삼되, 해당 PR이 공개 실행 로그로 통과를 증명한다고 쓰지 않습니다.

- [file-backed swap-in PR #171](https://github.com/tail1887/PINTOS-VM/pull/171)
- [메타데이터·swap-in 커밋 4f21ed1](https://github.com/tail1887/PINTOS-VM/commit/4f21ed1b15ada49f0ebe35b6c838b3c2162ab7d0)
- [실행 파일 page flag 보완 커밋 17f9740](https://github.com/tail1887/PINTOS-VM/commit/17f97404004179cb87b5de8924e16f30bca48189)

### 5.4 anon swap과 실패 롤백

후속 개인 커밋에서는 anonymous page의 swap slot 상태를 관리하고,
swap-in·swap-out과 slot 반환 경로를 연결했습니다.
page claim이 실패하면 부분적으로 연결된 frame 상태를 되돌리는 롤백도 보완했습니다.

추가로 mmap 공유 file을 한 번만 닫도록 소유권을 정리하고,
stack growth와 low-memory claim 실패 시 남은 frame 연결을 제거했습니다.

- [anon swap·claim rollback 커밋 8a980e1](https://github.com/tail1887/PINTOS-VM/commit/8a980e19bae8e80f1e6fefa92d083b64f794038b)
- [mmap file 소유권·stack rollback 커밋 b973394](https://github.com/tail1887/PINTOS-VM/commit/b9733941db9936140f3e217258bb0be6251af110)
- [low-memory rollback 보완 커밋 39eb637](https://github.com/tail1887/PINTOS-VM/commit/39eb637d4f67de881a1386a2dd6b409a8e62f28c)

## 6. 상태·소유권·실패 경로로 읽기

| 문제 | 관리한 상태 또는 소유권 | 실패 시 처리 |
| --- | --- | --- |
| Argument Passing | 임시 argv page와 사용자 스택 주소 | 중간 실패 시 임시 page 해제 |
| fork 파일 복제 | 새 file 객체와 FD table slot | 이미 복제한 file을 닫고 롤백 |
| 자식 대기 | child_status 참조 수와 semaphore | 부모·자식 종료 순서와 무관하게 정리 |
| file-backed page | file 위치·read bytes·zero bytes | 읽기 실패를 상위 claim 실패로 전달 |
| anon swap | page별 swap slot 점유 상태 | swap-in 후 slot 반환, claim 실패 시 연결 해제 |
| mmap file | 여러 page가 공유하는 file 수명 | 중복 close를 피하도록 소유권 정리 |

이 표가 Pintos 경험의 핵심입니다.
기능 목록보다 상태가 어디에 있고 누가 해제하며,
중간 실패 뒤 어떤 부분 상태가 남는지를 추적한 경험에 초점을 둡니다.

## 7. 확인된 범위와 확인되지 않은 범위

| 항목 | 상태 | 근거 수준 |
| --- | --- | --- |
| 개인 커밋의 작성자·diff | 확인 | Git commit과 실제 코드 |
| PR #123·#163·#168·#171 병합 | 확인 | GitHub PR 메타데이터 |
| PR #183의 VM 팀 통합 | 확인 | 팀 PR이며 개인 구현 근거로 사용하지 않음 |
| GitHub Actions 성공 | 제한적으로 확인 | timer-sleep 소스 계약 2건만 실행 |
| User Program·VM 전체 테스트 통과 | 확인 불가 | 공개 raw 실행 로그·전체 요약 없음 |
| PR #171 체크리스트 테스트 | 확인 불가 | 체크박스 미체크 |
| VM 전체 과제 완성 | 주장하지 않음 | 미완료·결함 범위가 남아 있음 |

현재 GitHub Actions의 unit-tests 워크플로는 Python 검사기 하나를 실행하며,
timer_sleep의 0 이하 tick guard와 ordered insert라는 소스 문자열 계약 2건만 확인합니다.
Pintos를 빌드하거나 User Program·VM 테스트를 실행하지 않으므로,
초록 체크를 User Program·VM의 실행 검증으로 해석하지 않습니다.

- [GitHub Actions 워크플로](https://github.com/tail1887/PINTOS-VM/blob/main/.github/workflows/unit-tests.yml)
- [실제로 실행되는 timer-sleep 계약 검사 2건](https://github.com/tail1887/PINTOS-VM/blob/main/unit-tests/test_timer_sleep_contract.py)

## 8. 성과에서 제외한 미완료 범위

PR #183은 92개 커밋을 포함한 팀 통합 PR이므로 개인 성과 링크로 사용하지 않습니다.

VM_UNINIT page의 fork 복사는 개인 작성 커밋이 병합되었지만,
현재 코드에는 저장소 hash API와 맞지 않는 순회 호출이 남아 있습니다.
file_reopen으로 늘어난 참조도 등록 실패·롤백 경로에서 완전히 닫히지 않을 가능성이 있습니다.
따라서 fork 시 lazy page 전체 복사와 실패 롤백을 완료한 성과로 기재하지 않습니다.

loaded ANON page 복사 PR은 병합되지 않았고,
loaded FILE·mmap page 전체 복사도 완료 범위로 확인되지 않습니다.

## 9. 근거를 읽는 순서

1. 이 문서의 30초 요약과 기여 경계로 주장 범위를 확인합니다.
2. User Program은 [Argument Passing 개념 노트](https://github.com/tail1887/PINTOS-VM/blob/76606fa97dcdb7a00167dac2ed11921441f03d6b/pintos/doc/taejung_files/2.%20week2/study/2.%20test-notes/argument_passing/1.%20feature/01-feature-overview-argument-flow.md) → 9d2ae31 → 7f8ce81 → 0886ffd 순으로 봅니다.
3. VM은 [Executable Lazy Load 개념 노트](https://github.com/tail1887/PINTOS-VM/blob/76606fa97dcdb7a00167dac2ed11921441f03d6b/pintos/doc/taejung_files/3.%20week3/study/3.%20test-notes/2.%20Anonymous%20Page/1.%20feature/03-feature-executable-lazy-load.md) → PR #163 → #168 → #171 순으로 상태 전환을 따라갑니다.
4. 후속 보완은 [File-backed Swap In 설계 노트](https://github.com/tail1887/PINTOS-VM/blob/76606fa97dcdb7a00167dac2ed11921441f03d6b/pintos/doc/taejung_files/3.%20week3/vm_master_plan/Merge%203%20-%20mmap%20-%20File-backed%20Page/C%20-%20File-backed%20Swap%20In.md)와 [Anonymous Swap 개념 노트](https://github.com/tail1887/PINTOS-VM/blob/76606fa97dcdb7a00167dac2ed11921441f03d6b/pintos/doc/taejung_files/3.%20week3/study/3.%20test-notes/5.%20Swap%20In%26Out/1.%20feature/01-feature-anonymous-swap-in-out.md)를 먼저 보고, 8a980e1 → b973394 → 39eb637 순으로 실패 롤백을 확인합니다.
5. PR #123과 #183은 개인 코드 증명이 아니라 팀 통합 맥락으로만 봅니다.
6. 테스트 주장은 Actions 파일과 PR 체크리스트를 함께 확인합니다.

개념·설계 노트는 과제 전체의 목표와 흐름을 이해하기 위한 보조 자료입니다. 개인 구현과 실제 검증 범위가 충돌하면 위에 연결한 개인 커밋, PR 메타데이터와 공개 워크플로를 우선합니다.

## 10. 면접에서 확인하기 좋은 코드 지점

- Argument Passing: process_exec의 인자 parsing과 stack 배치
- syscall 경계: 사용자 문자열·버퍼 검증 helper와 종료 경로
- 프로세스 생명주기: child_status, wait semaphore, FD 복제 롤백
- lazy loading: segment_aux 생성과 initializer 전달
- file-backed page: metadata 이전과 swap-in
- anon page: swap slot 점유·반환과 claim 실패 롤백

이 문서의 링크는 완성도를 과장하기 위한 장식이 아니라,
각 주장과 개인 기여 경계를 직접 확인하기 위한 읽기 경로입니다.
