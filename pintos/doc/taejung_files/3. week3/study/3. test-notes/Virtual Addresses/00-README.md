# Virtual Addresses 노트

이 폴더는 `Virtual Addresses.md`를 기준으로 Project 3에서 주소 계산과 검증 규칙을 확인하기 위한 보조 문서입니다.

## 원본 문서 위치

- `../../../../0. references/kaist_docs/project3/Virtual Addresses.md`

## 이 문서에서 확인할 것

- user virtual address와 kernel virtual address의 경계
- page offset, page number, page-aligned address
- `pg_round_down()`, `pg_round_up()` 사용 지점

## 구현 연결

- SPT key는 page-aligned user virtual address를 사용한다.
- page fault address는 원본 byte 주소로 들어오므로 lookup 전에 align한다.
- stack growth와 mmap validation은 user address 범위 검사를 먼저 한다.

## 빠른 체크 질문

- fault address와 SPT key 주소를 구분하는가?
- page boundary를 넘는 접근에서 어느 page들을 확인해야 하는가?
- kernel address를 user page로 등록하지 않는가?
