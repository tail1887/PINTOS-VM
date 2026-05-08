# Virtual Memory 테스트 노트 가이드

이 폴더는 `0. references/kaist_docs/project3`의 문서 파일명과 같은 순서로 Project 3 학습/구현 노트를 정리합니다.  
FAQ처럼 개발 순서에 직접 필요하지 않은 문서는 제외했습니다.

## 트랙 구성

1. `0. Introduction`
2. `1. Memory Management`
3. `2. Anonymous Page`
4. `3. Stack Growth`
5. `4. Memory Mapped Files`
6. `5. Swap In&Out`
7. `6. Copy-on-write (Extra)`
8. `Virtual Addresses`
9. `Hash Table`
10. `Page Table`

## 권장 개발 순서

1. `0. Introduction`으로 전체 소스 파일과 VM 용어를 확인한다.
2. `Virtual Addresses`, `Hash Table`, `Page Table`을 필요한 만큼 먼저 훑는다.
3. `1. Memory Management`에서 SPT, page structure, frame table 기본을 구현한다.
4. `2. Anonymous Page`에서 lazy loading과 anonymous page를 구현한다.
5. `3. Stack Growth`에서 합법적인 stack fault를 복구한다.
6. `4. Memory Mapped Files`에서 mmap/munmap과 file-backed page를 구현한다.
7. `5. Swap In&Out`에서 anonymous page eviction과 swap 복구를 완성한다.
8. `6. Copy-on-write (Extra)`는 기본 VM 테스트가 안정화된 뒤 진행한다.

## 폴더 역할

- `0. Introduction`: Project 3 전체 목표, 소스 파일, 용어
- `1. Memory Management`: page structure, SPT, frame allocation, eviction 기본
- `2. Anonymous Page`: uninit page, executable lazy load, anonymous page
- `3. Stack Growth`: rsp 기준 stack growth와 invalid access 경계
- `4. Memory Mapped Files`: mmap validation, file-backed page, munmap write-back
- `5. Swap In&Out`: anonymous page swap out/in과 swap slot 관리
- `6. Copy-on-write (Extra)`: fork page sharing과 write fault copy
- `Virtual Addresses`: 주소 계산과 page align 규칙
- `Hash Table`: SPT 구현용 Pintos hash table 사용법
- `Page Table`: pml4 mapping, accessed/dirty bit, SPT와의 경계

## 공통 템플릿/생성 규칙

- 통합 템플릿 위치: `../../../0. references/templates/test-notes`
- feature 문서: `<track>/1. feature`
- testing 문서: `<track>/2. testing`
- 보조 참고 문서: `<track>/00-README.md`
