# Lazy Loading/Anonymous Page 테스트 노트 인덱스

## 테스트 문서 목록

1. `01-lazy-load-executable.md` — executable segment lazy loading
2. `02-anonymous-page.md` — anonymous page 초기화와 swap 연결 전제

## 관련 feature 문서

- `../1. feature/01-feature-overview-lazy-loading.md`
- `../1. feature/02-feature-uninit-page-and-initializer.md`
- `../1. feature/03-feature-executable-lazy-load.md`
- `../1. feature/04-feature-anonymous-page.md`

## 우선 점검 순서

1. load 시점에 SPT entry만 등록되는지 확인한다.
2. fault 시점에 initializer가 한 번 실행되는지 확인한다.
3. file read bytes와 zero fill 범위가 정확한지 확인한다.
4. anonymous page가 file-backed page와 다른 eviction 정책을 갖는지 확인한다.
