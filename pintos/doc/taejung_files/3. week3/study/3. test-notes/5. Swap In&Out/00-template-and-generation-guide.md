# Swap In&Out 테스트 노트 템플릿/양식/생성 가이드

이 트랙은 `5. Swap In&Out.md`를 기준으로 anonymous page와 file-backed page가 eviction/fault 시점에 어떻게 저장되고 복구되는지 정리합니다.

## 1. 폴더 구조 기준

- 기능 문서: `5. Swap In&Out/1. feature`
- 테스트 문서: `5. Swap In&Out/2. testing`
- 공용 템플릿: `../../../../0. references/templates/test-notes`

## 2. 현재 표준 문서 세트

### feature 문서
- `01-feature-anonymous-swap-in-out.md`

### testing 문서
- `00-README.md`
- `01-swap-in-out.md`

## 3. 작성 규칙

- frame은 전역 자원, page는 process별 주소 공간 자원으로 분리해 쓴다.
- anonymous page는 swap, file-backed page는 file write-back/reload 대상이다.
- eviction 전후 pml4 mapping 제거를 반드시 기록한다.
