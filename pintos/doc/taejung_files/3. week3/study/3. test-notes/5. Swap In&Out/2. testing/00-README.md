# Swap In&Out 테스트 노트 인덱스

## 테스트 문서 목록

1. `01-swap-in-out.md` — anonymous page swap 저장/복구

## 관련 feature 문서

- `../1. feature/01-feature-anonymous-swap-in-out.md`

## 우선 점검 순서

1. anonymous page가 eviction 시 swap slot을 할당하는지 확인한다.
2. page 내용을 PGSIZE 단위로 swap disk에 기록하는지 확인한다.
3. fault 시 같은 slot에서 내용을 복구하는지 확인한다.
4. swap in 또는 page destroy 후 slot이 해제되는지 확인한다.
