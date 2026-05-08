# Stack Growth 테스트 노트 인덱스

## 테스트 문서 목록

1. `01-stack-growth.md` — rsp 근처 fault와 stack limit 검증

## 관련 feature 문서

- `../1. feature/01-feature-overview-stack-growth.md`
- `../1. feature/02-feature-fault-address-and-rsp.md`
- `../1. feature/03-feature-stack-limit-and-invalid-access.md`

## 우선 점검 순서

1. SPT miss인지 먼저 확인한다.
2. fault address가 user address인지 확인한다.
3. rsp 근처 접근인지 확인한다.
4. stack lower bound를 넘지 않는지 확인한다.
5. anonymous page 생성과 claim이 성공하는지 확인한다.
