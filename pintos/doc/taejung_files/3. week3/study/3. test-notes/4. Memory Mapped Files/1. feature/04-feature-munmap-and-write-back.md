# 04 — 기능 3: Munmap and Write-back

## 1. 구현 목적 및 필요성

### 이 기능이 무엇인가
`munmap` 또는 process exit에서 mmap range를 해제하고 dirty file-backed page를 파일에 반영하는 기능입니다.

### 왜 이걸 하는가
mmap으로 수정한 내용은 명시적 write syscall 없이도 파일에 저장되어야 합니다.

### 무엇을 연결하는가
`sys_munmap()`, mapping metadata, pml4 dirty bit, `file_backed_swap_out()`, SPT destroy를 연결합니다.

### 완성의 의미
dirty page는 실제 file bytes 범위만 write-back되고, clean page는 write 없이 정리됩니다.

## 2. 가능한 구현 방식 비교

- 방식 A: munmap에서 mapping range를 순회
  - 장점: syscall 단위 cleanup이 명확
  - 단점: metadata 필요
- 방식 B: SPT 전체에서 addr가 속한 page만 추적
  - 장점: 단순
  - 단점: 같은 mmap range 전체 해제가 어려움
- 선택: mapping range 기준 순회 권장

## 3. 시퀀스와 단계별 흐름

```mermaid
flowchart LR
  A[munmap addr] --> B[mapping range 찾기]
  B --> C[page별 dirty 확인]
  C --> D[write-back if dirty]
  D --> E[SPT remove/destroy]
```

## 4. 기능별 가이드

### 4.1 Dirty check
- 위치: `vm/file.c`, `vm/vm.c`
- pml4 dirty bit와 page 상태를 확인합니다.

### 4.2 Write-back
- 위치: `vm/file.c`
- 실제 file-backed bytes만 파일에 기록합니다.

## 5. 구현 주석

### 5.1 `sys_munmap()`

#### 5.1.1 mapping range cleanup
- 위치: `userprog/syscall.c` 또는 `vm/file.c`
- 역할: mmap range의 모든 page를 정리한다.
- 규칙 1: dirty page는 file에 반영한다.
- 규칙 2: SPT entry와 mapping metadata를 제거한다.
- 금지 1: 이미 munmap된 range를 다시 정리하지 않는다.

### 5.2 `file_backed_swap_out()` / destroy

#### 5.2.1 dirty write-back
- 위치: `vm/file.c`
- 역할: file-backed dirty page를 backing file에 기록한다.
- 규칙 1: write-back 길이는 read_bytes 또는 실제 file bytes 범위다.
- 규칙 2: clean page는 write하지 않는다.
- 금지 1: zero fill 영역까지 파일 크기를 잘못 늘리지 않는다.

## 6. 테스팅 방법

- mmap-write 테스트
- munmap write-back 테스트
- process exit write-back 테스트
- mmap clean page 회귀
