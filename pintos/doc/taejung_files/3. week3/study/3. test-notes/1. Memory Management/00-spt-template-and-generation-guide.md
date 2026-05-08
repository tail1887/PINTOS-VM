# Supplemental Page Table 테스트 노트 템플릿/양식/생성 가이드

이 트랙은 SPT를 새로 학습하거나 테스트 실패 원인을 정리할 때 같은 형식으로 문서를 확장하기 위한 기준입니다.

## 1. 폴더 구조 기준

- 기능 문서: `1. Memory Management/1. feature`
- 테스트 문서: `1. Memory Management/2. testing`
- 공용 템플릿: `../../../../0. references/templates/test-notes`

## 2. 현재 표준 문서 세트

### feature 문서
- `01-feature-overview-spt-flow.md`
- `02-feature-page-structure-and-hash-key.md`
- `03-feature-spt-insert-find-remove.md`
- `04-feature-spt-copy-and-destroy.md`

### testing 문서
- `00-README.md`
- `01-spt-basic-and-page-fault.md`
- `02-spt-copy-and-destroy.md`

## 3. 작성 규칙

- SPT와 pml4를 같은 것으로 설명하지 않는다.
- key는 항상 page-aligned user virtual address 기준으로 쓴다.
- page lifecycle은 allocate, insert, claim, destroy 순서로 분리해 기록한다.
- frame/eviction 세부 구현도 같은 `1. Memory Management` 트랙 안에서 함께 정리한다.

## 4. 빠른 체크리스트

- [ ] `pg_round_down()` 기준으로 lookup하는가?
- [ ] 중복 upage insert를 막는가?
- [ ] process init/fork/exit에서 SPT init/copy/destroy가 호출되는가?
- [ ] destroy 경로에서 page type별 cleanup이 호출되는가?
