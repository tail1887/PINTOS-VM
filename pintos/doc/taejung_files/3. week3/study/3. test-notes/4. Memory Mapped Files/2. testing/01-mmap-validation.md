# 01 — `mmap-validation` 검증

## 1. 이 테스트가 검증하는 것

mmap syscall이 잘못된 주소, 길이, fd, overlap range를 거부하고 유효한 mapping만 SPT에 등록하는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `sys_mmap`
- 위치: `userprog/syscall.c`
- 규칙 1: addr는 page-aligned user address여야 한다.
- 규칙 2: length 0, invalid fd, overlap range는 실패한다.

### SPT range check
- 위치: `vm/vm.c`, `vm/file.c`
- 규칙 1: mapping range의 모든 page에 대해 기존 entry를 검사한다.
- 규칙 2: 중간 실패 시 이미 등록한 page를 롤백한다.

## 3. 실패 시 바로 확인할 것

- addr가 NULL 또는 page-unaligned일 때 실패하는지
- fd 0/1 또는 닫힌 fd를 mmap하지 않는지
- range 일부만 겹쳐도 실패하는지

## 4. 재검증

- mmap validation 단일 테스트 통과 확인
- SPT insert/remove 회귀 재실행
