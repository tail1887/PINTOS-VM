# 01 — `spt-basic-and-page-fault` 검증

## 1. 이 테스트가 검증하는 것

lazy page가 SPT에 등록되고, page fault 발생 시 올바른 `struct page`를 찾아 claim하는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `spt_find_page`
- 위치: `vm/vm.c`
- 규칙 1: fault address를 page align한다.
- 규칙 2: 없으면 NULL을 반환하고 caller가 정책을 결정한다.

### `vm_try_handle_fault`
- 위치: `vm/vm.c`
- 규칙 1: SPT에 있는 page만 claim한다.
- 규칙 2: write fault는 writable 여부를 확인한다.

## 3. 실패 시 바로 확인할 것

- SPT key가 page-aligned va인지
- load 단계에서 lazy page insert가 성공했는지
- pml4 mapping 실패 후 page/frame 상태가 롤백되는지

## 4. 재검증

- 관련 VM lazy load 단일 테스트 통과 확인
- stack growth와 mmap lookup 테스트 재실행
