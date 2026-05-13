# Project 3 VM 마스터 플랜 (분할 문서)

원본 단일 파일(`project3_vm_complete_master_plan.md`)을 **분업·머지 단위**로 나눈 디렉터리다.

## 구조

| 구분 | 경로 |
|------|------|
| 분업 계획 §1–5 | [00-분업계획-섹션1-5/분업계획-섹션1-5.md](00-분업계획-섹션1-5/분업계획-섹션1-5.md) |
| 일정·리뷰·결론 (구 §11–14, 파일 내 §1–4) | [99-운영-리뷰-결론-섹션11-14/운영안-리뷰-결론-섹션11-14.md](99-운영-리뷰-결론-섹션11-14/운영안-리뷰-결론-섹션11-14.md) |

### Merge 1 – Frame Claim + Lazy Loading

폴더: [Merge 1 - Frame Claim + Lazy Loading](Merge%201%20-%20Frame%20Claim%20+%20Lazy%20Loading/)

- [00-서론.md](Merge%201%20-%20Frame%20Claim%20+%20Lazy%20Loading/00-서론.md) — 목표(§1), E2E(§1.1), 한 줄 순서(§1.2), 내부 머지(§2), 완료 기준(§3)
- [A - Frame Claim.md](Merge%201%20-%20Frame%20Claim%20+%20Lazy%20Loading/A%20-%20Frame%20Claim.md)
- [B - Uninit Page와 Initializer.md](Merge%201%20-%20Frame%20Claim%20+%20Lazy%20Loading/B%20-%20Uninit%20Page와%20Initializer.md)
- [C - Executable Segment Lazy Loading.md](Merge%201%20-%20Frame%20Claim%20+%20Lazy%20Loading/C%20-%20Executable%20Segment%20Lazy%20Loading.md)
- [D - 초기 Stack Page.md](Merge%201%20-%20Frame%20Claim%20+%20Lazy%20Loading/D%20-%20초기%20Stack%20Page.md)

### Merge 2 – Stack Growth + Page Cleanup

폴더: [Merge 2 - Stack Growth + Page Cleanup](Merge%202%20-%20Stack%20Growth%20+%20Page%20Cleanup/)

- [00-서론.md](Merge%202%20-%20Stack%20Growth%20+%20Page%20Cleanup/00-서론.md)
- [A - Stack Growth 판별.md](Merge%202%20-%20Stack%20Growth%20+%20Page%20Cleanup/A%20-%20Stack%20Growth%20판별.md)
- [B - Stack Growth 실행.md](Merge%202%20-%20Stack%20Growth%20+%20Page%20Cleanup/B%20-%20Stack%20Growth%20실행.md)
- [C - Page Destroy.md](Merge%202%20-%20Stack%20Growth%20+%20Page%20Cleanup/C%20-%20Page%20Destroy.md)
- [D - SPT Kill.md](Merge%202%20-%20Stack%20Growth%20+%20Page%20Cleanup/D%20-%20SPT%20Kill.md)

### Merge 3 – mmap / File-backed Page

폴더: [Merge 3 - mmap - File-backed Page](Merge%203%20-%20mmap%20-%20File-backed%20Page/)

- [00-서론.md](Merge%203%20-%20mmap%20-%20File-backed%20Page/00-서론.md)
- [A - mmap Validation.md](Merge%203%20-%20mmap%20-%20File-backed%20Page/A%20-%20mmap%20Validation.md)
- [B - mmap Page Registration.md](Merge%203%20-%20mmap%20-%20File-backed%20Page/B%20-%20mmap%20Page%20Registration.md)
- [C - File-backed Swap In.md](Merge%203%20-%20mmap%20-%20File-backed%20Page/C%20-%20File-backed%20Swap%20In.md)
- [D - munmap과 Write-back.md](Merge%203%20-%20mmap%20-%20File-backed%20Page/D%20-%20munmap과%20Write-back.md)

### Merge 4 – Eviction + Swap In/Out

폴더: [Merge 4 - Eviction + Swap In-Out](Merge%204%20-%20Eviction%20+%20Swap%20In-Out/)

- [00-서론.md](Merge%204%20-%20Eviction%20+%20Swap%20In-Out/00-서론.md)
- [A - Eviction Victim Selection.md](Merge%204%20-%20Eviction%20+%20Swap%20In-Out/A%20-%20Eviction%20Victim%20Selection.md)
- [B - Eviction Flow.md](Merge%204%20-%20Eviction%20+%20Swap%20In-Out/B%20-%20Eviction%20Flow.md)
- [C - Anonymous Swap Table.md](Merge%204%20-%20Eviction%20+%20Swap%20In-Out/C%20-%20Anonymous%20Swap%20Table.md)
- [D - File-backed Swap Out.md](Merge%204%20-%20Eviction%20+%20Swap%20In-Out/D%20-%20File-backed%20Swap%20Out.md)

### Merge 5 – Fork/Exit 정리 + 전체 안정화

폴더: [Merge 5 - Fork-Exit 정리 + 전체 안정화](Merge%205%20-%20Fork-Exit%20정리%20+%20전체%20안정화/)

- [00-서론.md](Merge%205%20-%20Fork-Exit%20정리%20+%20전체%20안정화/00-서론.md)
- [A - SPT Copy - Uninit Page.md](Merge%205%20-%20Fork-Exit%20정리%20+%20전체%20안정화/A%20-%20SPT%20Copy%20-%20Uninit%20Page.md)
- [B - SPT Copy - Loaded Anon Page.md](Merge%205%20-%20Fork-Exit%20정리%20+%20전체%20안정화/B%20-%20SPT%20Copy%20-%20Loaded%20Anon%20Page.md)
- [C - SPT Copy - File-backed-mmap Page.md](Merge%205%20-%20Fork-Exit%20정리%20+%20전체%20안정화/C%20-%20SPT%20Copy%20-%20File-backed-mmap%20Page.md)
- [D - Exit와 전체 Cleanup 안정화.md](Merge%205%20-%20Fork-Exit%20정리%20+%20전체%20안정화/D%20-%20Exit와%20전체%20Cleanup%20안정화.md)

## 참고

- 폴더 이름에 `/`는 넣지 않았다 (경로 이슈). Merge 3 제목은 원문과 동일하게 **mmap / File-backed Page** 의미다.
- **번호 체계**: 각 Merge 폴더·`99` 문서는 **파일마다 `## 1` … `### 1.1`**처럼 **1부터 다시 시작**한다. 옛 단일본 `6.3`, `8.5` 같은 장 번호는 쓰지 않고, 교차 참조는 **`A - ….md`**, **`00-서론.md` §2**, **`../다른 Merge/…`** 형태로 통일했다.
