# 03 — `munmap-and-exit-write-back` 검증

## 1. 이 테스트가 검증하는 것

dirty mmap page가 `munmap` 또는 process exit에서 파일에 정확히 반영되고, clean page는 불필요하게 쓰지 않는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `sys_munmap`
- 위치: `userprog/syscall.c` 또는 `vm/file.c`
- 규칙 1: mapping range의 모든 page를 순회한다.
- 규칙 2: page별 cleanup 후 metadata를 제거한다.

### `file_backed_swap_out`
- 위치: `vm/file.c`
- 규칙 1: dirty page만 file에 write-back한다.
- 규칙 2: write length는 실제 file-backed bytes 범위로 제한한다.

## 3. 실패 시 바로 확인할 것

- process exit에서도 munmap과 같은 cleanup이 실행되는지
- dirty bit를 확인할 때 pml4 mapping이 아직 존재하는지
- 같은 page가 munmap과 SPT destroy에서 두 번 write/free되지 않는지

## 4. 재검증

- munmap/process exit write-back 테스트 통과 확인
- mmap clean, swap eviction 회귀 재실행
