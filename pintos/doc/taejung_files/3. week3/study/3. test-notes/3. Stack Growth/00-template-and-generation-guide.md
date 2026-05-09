# Stack Growth 테스트 노트 템플릿/양식/생성 가이드

이 트랙은 page fault가 stack 확장으로 복구될 수 있는지 판정하는 규칙을 정리합니다.

## 1. 폴더 구조 기준

- 기능 문서: `3. Stack Growth/1. feature`
- 테스트 문서: `3. Stack Growth/2. testing`
- 공용 템플릿: `../../../../0. references/templates/test-notes`

## 2. 현재 표준 문서 세트

### feature 문서
- `01-feature-overview-stack-growth.md`
- `02-feature-fault-address-and-rsp.md`
- `03-feature-stack-limit-and-invalid-access.md`

### testing 문서
- `00-README.md`
- `01-stack-growth.md`

## 3. 작성 규칙

- stack growth 판정은 SPT miss 이후에만 다룬다.
- 허용 조건을 너무 넓게 잡지 않는다.
- 생성되는 page type은 anonymous page로 기록한다.
- deep-dive 문서 5장은 `규칙=불변식`, `구현 체크 순서=실제 구현 단계`로 분리해 작성한다.
