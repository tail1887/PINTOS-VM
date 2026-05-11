# Memory Mapped Files 테스트 노트 템플릿/양식/생성 가이드

이 트랙은 `mmap`, file-backed page, `munmap`, dirty write-back을 정리합니다.

## 1. 폴더 구조 기준

- 기능 문서: `4. Memory Mapped Files/1. feature`
- 테스트 문서: `4. Memory Mapped Files/2. testing`
- 공용 템플릿: `../../../../0. references/templates/test-notes`

## 2. 현재 표준 문서 세트

### feature 문서
- `01-feature-overview-mmap-flow.md`
- `02-feature-mmap-validation-and-registration.md`
- `03-feature-file-backed-page-load.md`
- `04-feature-munmap-and-write-back.md`

### testing 문서
- `00-README.md`
- `01-mmap-validation.md`
- `02-mmap-read-write.md`
- `03-munmap-and-exit-write-back.md`

## 3. 작성 규칙

- mmap은 file-backed lazy page 등록 기능으로 설명한다.
- fd close와 mapping 수명은 분리해서 기록한다.
- dirty page write-back 범위는 실제 file bytes 기준으로 쓴다.
- deep-dive 문서 5장은 `규칙=불변식`, `구현 체크 순서=실제 구현 단계`로 분리해 작성한다.
