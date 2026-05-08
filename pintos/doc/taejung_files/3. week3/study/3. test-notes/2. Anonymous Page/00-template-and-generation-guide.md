# Lazy Loading/Anonymous Page 테스트 노트 템플릿/양식/생성 가이드

이 트랙은 executable lazy loading, uninit page, anonymous page를 정리합니다.

## 1. 폴더 구조 기준

- 기능 문서: `2. Anonymous Page/1. feature`
- 테스트 문서: `2. Anonymous Page/2. testing`
- 공용 템플릿: `../../../../0. references/templates/test-notes`

## 2. 현재 표준 문서 세트

### feature 문서
- `01-feature-overview-lazy-loading.md`
- `02-feature-uninit-page-and-initializer.md`
- `03-feature-executable-lazy-load.md`
- `04-feature-anonymous-page.md`

### testing 문서
- `00-README.md`
- `01-lazy-load-executable.md`
- `02-anonymous-page.md`

## 3. 작성 규칙

- load 시점과 fault 시점을 반드시 분리한다.
- aux에는 fault 시점까지 필요한 file/ofs/read_bytes/zero_bytes/writable 정보를 남긴다.
- anonymous page와 file-backed page의 eviction 정책을 섞지 않는다.
