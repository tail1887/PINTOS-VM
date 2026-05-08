# 01 — `stack-growth` 검증

## 1. 이 테스트가 검증하는 것

사용자 스택 근처의 합법적인 fault가 anonymous page 생성으로 복구되는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `vm_try_handle_fault`
- 위치: `vm/vm.c`
- 규칙 1: SPT miss일 때만 stack growth를 고려한다.
- 규칙 2: rsp 근처와 stack limit 조건을 모두 만족해야 한다.

### `vm_stack_growth`
- 위치: `vm/vm.c`
- 규칙 1: page-aligned fault address에 anonymous page를 만든다.
- 규칙 2: 생성 후 claim까지 성공해야 한다.

## 3. 실패 시 바로 확인할 것

- rsp 값이 user rsp인지
- fault address와 rsp 비교 조건이 너무 좁거나 넓지 않은지
- stack page가 writable anonymous page인지

## 4. 재검증

- stack growth 단일 테스트 통과 확인
- bad pointer와 swap 회귀 재실행
