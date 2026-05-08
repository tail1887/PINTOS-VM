# Copy-on-write (Extra) 노트

이 폴더는 `6. Copy-on-write (Extra).md`를 기준으로 extra 구현을 진행할 때 사용합니다.

## 원본 문서 위치

- `../../../../0. references/kaist_docs/project3/6. Copy-on-write (Extra).md`

## 진행 전 전제

- `1. Memory Management`의 SPT/frame/page lifecycle이 안정적이어야 한다.
- `2. Anonymous Page`의 page type별 initializer와 swap 정책이 안정적이어야 한다.
- fork에서 SPT copy가 기본 방식으로 먼저 동작해야 한다.

## 구현 포인트

- fork 시 page 내용을 즉시 복사하지 않고 frame을 공유한다.
- 공유 page는 read-only로 매핑한다.
- write fault가 발생하면 private frame을 새로 만들고 내용을 복사한다.
- frame 또는 page에 refcount를 두어 마지막 참조가 사라질 때만 해제한다.

## 실패 시 점검

- write fault와 일반 permission fault를 구분하는가?
- 부모/자식 중 한쪽 write가 다른 쪽 메모리를 바꾸지 않는가?
- refcount가 0이 되기 전 frame을 free하지 않는가?
