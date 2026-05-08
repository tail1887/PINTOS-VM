# 02 — `mmap-read-write` 검증

## 1. 이 테스트가 검증하는 것

mmap된 주소를 읽을 때 파일 내용이 보이고, 쓰기 가능한 mapping에 쓴 내용이 dirty page로 추적되는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `file_backed_swap_in`
- 위치: `vm/file.c`
- 규칙 1: file offset에서 read_bytes만큼 읽는다.
- 규칙 2: partial page의 남은 영역을 zero fill한다.

### dirty bit 관리
- 위치: `vm/file.c`, `vm/vm.c`
- 규칙 1: pml4 dirty bit를 기준으로 write-back 여부를 판단한다.
- 규칙 2: read-only mapping에 write fault가 나면 실패해야 한다.

## 3. 실패 시 바로 확인할 것

- file reopen이 되어 fd close 후에도 read 가능한지
- writable bit가 pml4 mapping에 반영되는지
- partial page가 0으로 채워지는지

## 4. 재검증

- mmap read/write 단일 테스트 통과 확인
- munmap write-back 테스트 재실행
