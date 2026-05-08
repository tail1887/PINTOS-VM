# 01 — `lazy-load-executable` 검증

## 1. 이 테스트가 검증하는 것

실행 파일 segment가 load 시점에 SPT lazy page로 등록되고, 실제 접근 시 파일에서 정확히 로드되는지 검증합니다.

## 2. 구현 필요 함수와 규칙

### `load_segment`
- 위치: `userprog/process.c`
- 규칙 1: page별 lazy initializer와 aux를 등록한다.
- 규칙 2: read_bytes/zero_bytes/writable 정보를 보존한다.

### `lazy_load_segment`
- 위치: `userprog/process.c`
- 규칙 1: file offset 기준으로 정확한 byte 수를 읽는다.
- 규칙 2: 남은 영역은 zero fill한다.

## 3. 실패 시 바로 확인할 것

- aux가 fault 시점까지 살아 있는지
- file offset이 page마다 증가하는지
- pml4 writable bit가 segment 권한과 일치하는지

## 4. 재검증

- 기본 실행 테스트 통과 확인
- SPT basic, page boundary 테스트 재실행
