# 01 — `frame-allocation-and-eviction` 검증

## 1. 이 테스트가 검증하는 것

page claim 과정에서 frame이 정확히 할당되고, 부족하면 victim eviction으로 새 frame을 확보하는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `vm_get_frame`
- 위치: `vm/vm.c`
- 규칙 1: user pool에서 frame을 할당한다.
- 규칙 2: 실패 시 eviction 경로로 진입한다.

### `vm_evict_frame`
- 위치: `vm/vm.c`
- 규칙 1: victim page 내용을 backing store에 보존한다.
- 규칙 2: pml4 mapping과 page-frame 연결을 정리한다.

## 3. 실패 시 바로 확인할 것

- `PAL_USER` 사용 여부
- accessed bit가 계속 true로 남아 victim 선정이 멈추는지
- evicted page의 pml4 mapping이 제거되었는지

## 4. 재검증

- swap 테스트 통과 확인
- mmap dirty page eviction 회귀 재실행
