# Memory Mapped Files 테스트 노트 인덱스

## 테스트 문서 목록

1. `01-mmap-validation.md` — mmap 인자 검증과 range 등록
2. `02-mmap-read-write.md` — file-backed page load와 수정
3. `03-munmap-and-exit-write-back.md` — munmap/process exit write-back

## 관련 feature 문서

- `../1. feature/01-feature-overview-mmap-flow.md`
- `../1. feature/02-feature-mmap-validation-and-registration.md`
- `../1. feature/03-feature-file-backed-page-load.md`
- `../1. feature/04-feature-munmap-and-write-back.md`

## 우선 점검 순서

1. mmap syscall 검증이 명세와 맞는지 확인한다.
2. SPT overlap 검사가 page 단위로 되는지 확인한다.
3. fault 시 file page가 정확히 로드되는지 확인한다.
4. dirty page만 write-back되는지 확인한다.
