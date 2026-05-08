# 02 — `swap-in-out` 검증

## 1. 이 테스트가 검증하는 것

anonymous page가 eviction 시 swap disk에 저장되고, 다시 접근할 때 같은 내용으로 복구되는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `anon_swap_out`
- 위치: `vm/anon.c`
- 규칙 1: free swap slot을 할당한다.
- 규칙 2: PGSIZE 전체를 swap disk에 기록한다.

### `anon_swap_in`
- 위치: `vm/anon.c`
- 규칙 1: page가 소유한 slot에서 PGSIZE 전체를 읽는다.
- 규칙 2: 성공 후 slot을 해제한다.

## 3. 실패 시 바로 확인할 것

- slot index와 sector 계산이 맞는지
- swap in 후 bitmap bit가 free로 돌아가는지
- page destroy에서 evicted page의 slot이 해제되는지

## 4. 재검증

- swap 단일 테스트 통과 확인
- stack growth와 fork 회귀 재실행
