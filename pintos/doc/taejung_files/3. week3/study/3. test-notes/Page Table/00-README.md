# Page Table 노트

이 폴더는 `Page Table.md`를 기준으로 pml4 page table 조작과 SPT의 경계를 정리합니다.

## 원본 문서 위치

- `../../../../0. references/kaist_docs/project3/Page Table.md`

## 이 문서에서 확인할 것

- pml4 생성/파괴/활성화 흐름
- `pml4_get_page`, `pml4_set_page`, `pml4_clear_page`
- accessed bit와 dirty bit 확인/초기화

## 구현 연결

- SPT는 "있어야 할 page"를 설명하고, pml4는 "현재 frame에 매핑된 page"를 표현한다.
- claim 성공 후에만 `pml4_set_page()`를 호출한다.
- eviction 전에는 dirty/accessed bit를 확인하고, eviction 후에는 mapping을 제거한다.

## 빠른 체크 질문

- SPT에 page가 있어도 pml4에는 아직 없을 수 있다는 점을 구분하는가?
- eviction 대상 page의 pml4 mapping을 제거하는가?
- dirty bit를 확인할 때 file-backed page write-back 범위와 연결하는가?
