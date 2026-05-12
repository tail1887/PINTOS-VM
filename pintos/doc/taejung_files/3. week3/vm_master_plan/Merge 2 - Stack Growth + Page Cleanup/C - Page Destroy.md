# C – Page Destroy

## 1. 개요 (목표·이유·수정 위치·의존성)

```text
목표
- page 타입별 destroy 함수를 구현해 page가 가진 자원을 정리한다.

이유
- free(page)만 하면 file/swap/frame 같은 부가 자원이 남을 수 있다.

수정/추가 위치
- vm/uninit.c
  - uninit_destroy()
- vm/anon.c
  - anon_destroy()
- vm/file.c
  - file_backed_destroy() 초안 확인

의존성
- D의 supplemental_page_table_kill이 destroy(page)를 호출한다.
- Merge 4 이후 swap slot 정리 로직과 연결된다.
```

## 2. 시퀀스

`vm_dealloc_page` 또는 타입별 경로에서 **`destroy(page)`**가 호출되면, `page_operations`에 매달린 **uninit/anon/file `destroy`**가 자원을 푼다.

```mermaid
sequenceDiagram
	autonumber
	participant Caller as vm_dealloc_or_kill
	participant P as struct_page
	participant Op as destroy_vtable
	participant R as frame_file_swap

	Caller->>P: destroy 요청
	P->>Op: page 타입에 맞는 destroy
	Op->>R: frame 해제·파일 ref·swap 등
	R-->>Op: 정리
	Op-->>Caller: page free 전 단계
```

## 3. 단계별 설명 (이 문서 범위)

1. **역할**: `free(page)` 전에 **타입별 부가 자원**을 모은다.
2. **호출자**: eviction·munmap·`supplemental_page_table_kill` 등이 공통으로 탄다.
3. **Merge 4**: anon swap 슬롯 해제 등은 Merge 4 폴더에서 두꺼워진다.

## 4. 구현 주석 가이드

### 4.1 구현 대상 함수 목록

- `uninit_destroy` (`vm/uninit.c`)
- `anon_destroy` (`vm/anon.c`)
- `file_backed_destroy` (`vm/file.c`)
- (호출 경로) `vm_dealloc_page`의 `destroy(page)` 매크로

### 4.2 공통 구조체/필드 계약

- `destroy(page)`는 `page->operations->destroy(page)`로 호출된다.
- 각 destroy는 자기 타입 자원만 정리하고 `free(page)`는 호출하지 않는다.
- `page->frame` 연계 자원 해제는 중복 호출 방지 규약을 지킨다.
- Merge 2 범위에서는 swap slot reclaim 세부를 과도하게 확장하지 않는다.

### 4.3 함수별 구현 주석 (고정안)

#### `uninit_destroy` / `anon_destroy` / `file_backed_destroy`

**추상**

```c
/* Merge2-C: 페이지 타입별 부가 자원만 정리하고, 공통 해제(free(page))는 호출자(vm_dealloc_page)에 맡긴다. */
```

**1단계 구체**

- uninit: 남아 있는 `aux`/초기화 정보 정리.
- anon: frame 역참조/익명 페이지 자원 정리.
- file-backed: dirty/write-back 정책은 D/후속 Merge와 충돌 없게 최소 정리.

**2단계 구체**

1. destroy 진입 시 타입에 맞는 구조체 포인터를 얻는다.
2. 이미 해제된 자원은 다시 해제하지 않도록 가드한다.
3. 필요 시 `page->frame = NULL` 등 링크 정리.
4. `return;` (메모리 자체 해제는 상위 호출자가 수행).
5. **하지 않음**: SPT 순회, hash destroy 전체, 프로세스 종료 분기.

### 4.4 함수 간 연결 순서 (호출 체인)

1. `vm_dealloc_page` 또는 D의 kill 경로가 `destroy(page)`를 호출한다.
2. vtable이 타입별 destroy로 분기한다.
3. destroy 완료 후 호출자가 `free(page)`를 수행한다.

### 4.5 실패 처리/롤백 규칙

- destroy 내부는 실패를 반환하지 않으므로 idempotent(중복 안전)하게 작성한다.
- write-back 실패 등 정책성 실패는 D/후속 Merge에서 처리 위치를 고정한다.
- C에서는 예외 발생 시에도 double free가 나지 않게 링크 정리를 우선한다.

### 4.6 완료 체크리스트

- 타입별 destroy 함수가 모두 구현되어 있다.
- `free(page)`가 destroy 내부가 아니라 호출자에서 1회만 수행된다.
- 종료/eviction/munmap 공통 경로에서 destroy를 재사용할 수 있다.
- double free 경로가 문서 상으로 차단되어 있다.
