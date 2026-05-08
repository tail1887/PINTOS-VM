# 02 — `spt-copy-and-destroy` 검증

## 1. 이 테스트가 검증하는 것

fork/exit 경로에서 SPT가 올바르게 복사되고 모든 page 자원이 한 번씩 정리되는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `supplemental_page_table_copy`
- 위치: `vm/vm.c`
- 규칙 1: 부모 page 포인터를 그대로 공유하지 않는다.
- 규칙 2: page type, writable, backing 정보를 보존한다.

### `supplemental_page_table_kill`
- 위치: `vm/vm.c`
- 규칙 1: page type별 destroy hook을 호출한다.
- 규칙 2: frame/swap/file 자원을 중복 해제하지 않는다.

## 3. 실패 시 바로 확인할 것

- child SPT init이 copy보다 먼저 수행되는지
- loaded page 내용 복사 또는 COW 정책이 일관적인지
- exit와 munmap cleanup이 같은 page를 두 번 정리하지 않는지

## 4. 재검증

- fork 관련 VM 테스트 통과 확인
- mmap exit, swap cleanup 회귀 재실행
