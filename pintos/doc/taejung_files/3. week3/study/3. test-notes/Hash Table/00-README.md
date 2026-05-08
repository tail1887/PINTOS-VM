# Hash Table 노트

이 폴더는 `Hash Table.md`를 기준으로 SPT 구현에 쓰는 Pintos hash table 사용법을 정리합니다.

## 원본 문서 위치

- `../../../../0. references/kaist_docs/project3/Hash Table.md`

## 이 문서에서 확인할 것

- `struct hash`, `struct hash_elem`
- hash function과 less function 작성 방식
- `hash_insert`, `hash_find`, `hash_delete`, `hash_destroy`
- `hash_entry()`로 container 구조체를 복원하는 방식

## 구현 연결

- SPT는 process별 hash table로 구현할 수 있다.
- hash key는 `struct page.va`로 둔다.
- find용 임시 `struct page`를 만들 때도 va를 page align한다.

## 빠른 체크 질문

- `hash_elem`은 `struct page` 안에 들어 있는가?
- hash/equal 비교 기준이 page-aligned va 하나로 고정되어 있는가?
- destroy callback에서 page 자원까지 정리하는가?
