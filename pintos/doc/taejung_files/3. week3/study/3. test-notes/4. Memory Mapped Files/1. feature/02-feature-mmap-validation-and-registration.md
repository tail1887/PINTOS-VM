# 02 — 기능 1: Mmap Validation and Registration

## 1. 구현 목적 및 필요성

### 이 기능이 무엇인가
`mmap` syscall 인자를 검증하고, 파일 범위를 page 단위 file-backed lazy page로 SPT에 등록하는 기능입니다.

### 왜 이걸 하는가
잘못된 주소나 이미 사용 중인 range를 허용하면 SPT와 pml4 상태가 깨집니다.

### 무엇을 연결하는가
`sys_mmap()`, fd table, `file_reopen()`, SPT insert, file-backed page initializer를 연결합니다.

### 완성의 의미
유효한 mmap은 시작 주소를 반환하고, 실패 조건은 `NULL` 또는 명세된 실패값으로 처리됩니다.

## 2. 가능한 구현 방식 비교

- 방식 A: mmap metadata를 별도 list로 관리
  - 장점: munmap(addr)에서 range 순회가 쉬움
  - 단점: SPT와 중복 상태 관리 필요
- 방식 B: SPT만 보고 munmap
  - 장점: 구조가 단순
  - 단점: mapping 단위 경계 추적이 어려움
- 선택: 팀 구현 상황에 맞추되, range 경계는 반드시 추적한다.

## 3. 시퀀스와 단계별 흐름

```mermaid
flowchart LR
  A[mmap syscall] --> B[addr/length/fd 검증]
  B --> C[file reopen]
  C --> D[page별 aux 생성]
  D --> E[SPT insert]
```

## 4. 기능별 가이드

### 4.1 Validation
- 위치: `userprog/syscall.c`
- addr align, length, fd, overlap을 확인합니다.

### 4.2 Registration
- 위치: `vm/file.c`, `vm/vm.c`
- page별 file-backed lazy page를 SPT에 등록합니다.

## 5. 구현 주석

### 5.1 `sys_mmap()`

#### 5.1.1 mmap 인자 검증
- 위치: `userprog/syscall.c`
- 역할: 사용자 mmap 요청이 유효한지 판단한다.
- 규칙 1: addr는 NULL이 아니고 page-aligned여야 한다.
- 규칙 2: length는 0보다 커야 한다.
- 규칙 3: fd는 mmap 가능한 열린 파일이어야 한다.
- 금지 1: 이미 SPT에 page가 있는 range를 덮어쓰지 않는다.

### 5.2 file-backed page 등록

#### 5.2.1 page별 SPT insert
- 위치: `vm/file.c`
- 역할: file range를 page 단위 lazy page로 등록한다.
- 규칙 1: page마다 file offset/read_bytes/zero_bytes를 보존한다.
- 규칙 2: fd close와 독립되도록 file 수명을 관리한다.
- 금지 1: syscall 시점에 파일 전체를 미리 읽지 않는다.

## 6. 테스팅 방법

- mmap invalid address/fd/length 테스트
- mmap overlap 테스트
- close-after-mmap 테스트
