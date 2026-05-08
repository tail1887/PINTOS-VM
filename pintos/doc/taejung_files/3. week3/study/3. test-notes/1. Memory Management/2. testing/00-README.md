# Memory Management 테스트 노트 인덱스

이 폴더는 `1. Memory Management.md` 기준으로 SPT, page lifecycle, frame table, eviction 기본을 기능별로 묶어 정리합니다.

## 테스트 문서 목록

1. `01-spt-basic-and-page-fault.md` — lazy page lookup과 page fault 복구
2. `02-spt-copy-and-destroy.md` — fork/exit에서 SPT copy/destroy
3. `03-frame-allocation-and-eviction.md` — frame claim과 victim 선정

## 관련 feature 문서

- `../1. feature/01-feature-overview-spt-flow.md`
- `../1. feature/02-feature-page-structure-and-hash-key.md`
- `../1. feature/03-feature-spt-insert-find-remove.md`
- `../1. feature/04-feature-spt-copy-and-destroy.md`
- `../1. feature/05-feature-overview-frame-table.md`
- `../1. feature/06-feature-frame-allocation-and-claim.md`
- `../1. feature/07-feature-eviction-and-accessed-bit.md`

## 우선 점검 순서

1. SPT init이 process 시작 시 호출되는지 확인한다.
2. load/mmap/stack growth가 SPT entry를 생성하는지 확인한다.
3. page fault가 `pg_round_down()` 주소로 lookup하는지 확인한다.
4. process exit에서 모든 page가 destroy되는지 확인한다.
5. frame이 `PAL_USER`로 할당되고 page-frame 연결이 유지되는지 확인한다.
