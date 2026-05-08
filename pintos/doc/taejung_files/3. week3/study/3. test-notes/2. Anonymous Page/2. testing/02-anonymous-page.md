# 02 — `anonymous-page` 검증

## 1. 이 테스트가 검증하는 것

anonymous page가 zero-filled page로 초기화되고, stack growth와 swap의 기본 page type으로 사용 가능한지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `anon_initializer`
- 위치: `vm/anon.c`
- 규칙 1: anonymous operation table을 설정한다.
- 규칙 2: 최초 page 내용은 zero 상태여야 한다.

### `anon_swap_in` / `anon_swap_out`
- 위치: `vm/anon.c`
- 규칙 1: swap slot 상태를 page metadata와 동기화한다.
- 규칙 2: slot 해제 누락이 없어야 한다.

## 3. 실패 시 바로 확인할 것

- stack growth가 anonymous page를 만들고 있는지
- swap slot 초기값과 사용 중 상태가 구분되는지
- page destroy에서 남은 swap slot을 해제하는지

## 4. 재검증

- stack growth 테스트 통과 확인
- swap 관련 테스트 재실행
