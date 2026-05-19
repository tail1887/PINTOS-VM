# 머지 5 B 정리 — SPT Copy: Loaded Anon Page

자, 이번 머지 5 B에서는 **fork 시 부모 SPT에 이미 올라와 있는 anonymous page(loaded anon)의 내용을 자식에게 독립적으로 복사**하는 데 있습니다.

A에서 UNINIT(lazy) page는 “나중에 fault 때 채울 정보”만 넘겼다면, B는 **이미 materialize된 anon** 을 다룹니다. 대표 예는 `setup_stack` 직후 **스택 bottom을 `vm_claim_page`로 올려 둔 page** 입니다. exec lazy segment는 아직 `VM_UNINIT`이라 A가 담당하고, 스택처럼 **이미 `VM_ANON`으로 바뀐 page** 가 B의 대상입니다.

프로젝트 2의 `duplicate_pte`는 부모 물리 페이지를 `memcpy`로 자식에 복사했습니다. VM에서도 fork 후 **부모·자식이 같은 frame kva를 공유하면 안 됩니다.** (COW는 extra — 이번 B는 **즉시 복사** 정책입니다.)

---

## A와 B의 차이 (한눈에)

| | A (UNINIT) | B (loaded anon) |
|---|-----------|-----------------|
| `operations->type` | `VM_UNINIT` | `VM_ANON` |
| frame | 없음 | 있거나 swap out됨 |
| 자식에 넣는 것 | lazy 메타(`init`, `aux`) | **실제 4KB 내용** |
| 핵심 API | `vm_alloc_page_with_initializer` | `vm_alloc_page` + `vm_claim_page` + `memcpy` |
| swap 슬롯 | 복사 안 함 | 부모만 swap out이면 **디스크에서 읽기** |

---

## B가 맡는 것 / 맡지 않는 것

**맡는 것**

- 부모 `operations->type == VM_ANON` 인 page
- 자식 SPT에 anon 등록 → claim → **PGSIZE 복사**
- 부모가 swap out만 된 경우(`frame == NULL`, `swap_slot` 있음) 디스크 스냅샷 읽기

**맡지 않는 것 (B 경계)**

- `VM_FILE` / mmap 메타 복사 → **Merge 5-C**
- 부모·자식 **frame 포인터 공유** (COW PTE 보호)
- swap 슬롯 **공유·refcount** (자식은 새 frame에만 내용 복사, 부모 슬롯은 그대로 둠)
- `supplemental_page_table_kill` / exit 정리 순서 → **D**

---

## 전체 호출 흐름

A가 붙은 뒤 `supplemental_page_table_copy`는 타입별로 갈라집니다.

```text
supplemental_page_table_copy(dst, src)
  └─ hash 순회 (부모 SPT)
       ├─ VM_UNINIT  → spt_copy_uninit_page        (A)
       ├─ VM_ANON    → spt_copy_loaded_anon_page   (B)  ← 이 문서
       └─ VM_FILE 등 → false + spt_copy_rollback   (C 미구현)
```

B 한 페이지의 내부 흐름:

```text
spt_copy_loaded_anon_page(src)
  ├─ vm_alloc_page(VM_ANON, va, writable)
  ├─ vm_claim_page(va)
  ├─ child = spt_find_page(자식 spt, va)
  └─ 내용 복사
        ├─ src->frame != NULL     → memcpy(child_kva, parent_kva, PGSIZE)
        ├─ src->swap_slot 유효    → anon_read_swap_to_kva(slot, child_kva)
        └─ 둘 다 아니면           → spt_remove_page, false
```

---

## 왜 `vm_alloc_page` 다음에 `vm_claim_page`인가?

`vm_alloc_page`는 매크로로 Merge 1 경로를 탑니다.

```c
#define vm_alloc_page(type, upage, writable) \
	vm_alloc_page_with_initializer ((type), (upage), (writable), NULL, NULL)
```

anon의 경우 내부에서 `uninit_new` → SPT insert까지 하고, 아직 **유저 PTE·frame은 없습니다.**

`vm_claim_page(va)`가 Merge 1 claim 몸체(`vm_do_claim_page`)를 호출합니다.

1. `vm_get_frame()` — 자식 전용 frame
2. `pml4_set_page` — 자식 `pml4`에 매핑
3. `swap_in` → `anon_swap_in` — 슬롯이 없으면 **true만 반환** (내용은 palloc된 frame, 아직 0일 수 있음)

그 다음 B에서 **`memcpy`로 부모 내용을 덮어씁니다.** claim 직후 frame이 비어 있어도 괜찮습니다.

---

## 부모 page 상태 두 가지

loaded anon이라도 부모 쪽 물리 상태는 두 갈래입니다.

### ① RAM에 있음 (`src->frame != NULL`)

가장 흔한 경우입니다. `setup_stack` 직후 스택, stack growth로 claim된 anon 등.

```c
memcpy (child_kva, src->frame->kva, PGSIZE);
```

부모 `frame->kva`는 **커널 가상 주소**라서, `thread_current()`가 자식이어도 부모 frame 메모리는 그대로 읽을 수 있습니다. (부모 프로세스가 아직 살아 있는 fork 시점)

### ② swap out만 됨 (`frame == NULL`, `swap_slot != NONE`)

저메모리 eviction 이후 fork하면 부모 anon은 디스크 슬롯에만 내용이 있습니다.

여기서 **`anon_swap_in`을 자식 page에 그대로 쓰면 안 됩니다.**

- `anon_swap_in`은 읽은 뒤 **`bitmap_reset`으로 슬롯을 비웁니다.**
- 부모도 같은 슬롯 번호를 들고 있으면, 자식이 슬롯을 먹어버리면 부모 쪽이 깨집니다.

그래서 B 전용 헬퍼 **`anon_read_swap_to_kva`** 를 둡니다.

- 디스크에서만 읽음
- **부모 슬롯·비트맵은 건드리지 않음**
- 자식은 RAM에만 내용을 갖고, `swap_slot == NONE` 유지

```c
bool
anon_read_swap_to_kva (size_t swap_slot, void *kva) {
	if (swap_slot == ANON_SWAP_SLOT_NONE)
		return false;

	disk_sector_t base = swap_slot * (PGSIZE / DISK_SECTOR_SIZE);
	for (size_t i = 0; i < PGSIZE / DISK_SECTOR_SIZE; i++)
		disk_read (swap_disk, base + i, kva + i * DISK_SECTOR_SIZE);
	return true;
}
```

`anon_swap_in`과 디스크 읽기 루프는 같지만, **락·bitmap_reset·page 필드 수정이 없다**는 점이 다릅니다.

### ③ `frame == NULL` 이고 슬롯도 없음

비정상 상태입니다. 등록만 하고 claim만 된 적 없는 anon은 보통 UNINIT이어야 합니다. **`false` + `spt_remove_page`** 로 자식 엔트리를 되돌립니다.

---

## `spt_copy_loaded_anon_page` 구현

```c
static bool
spt_copy_loaded_anon_page (struct page *src) {
	void *va = src->va;
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *child = NULL;
	void *child_kva = NULL;

	if (!vm_alloc_page (VM_ANON, va, src->writable))
		return false;

	if (!vm_claim_page (va)) {
		child = spt_find_page (spt, va);
		if (child != NULL)
			spt_remove_page (spt, child);
		return false;
	}

	child = spt_find_page (spt, va);
	if (child == NULL || child->frame == NULL)
		return false;

	child_kva = child->frame->kva;

	if (src->frame != NULL) {
		memcpy (child_kva, src->frame->kva, PGSIZE);
	} else if (src->anon.swap_slot != ANON_SWAP_SLOT_NONE) {
		if (!anon_read_swap_to_kva (src->anon.swap_slot, child_kva)) {
			spt_remove_page (spt, child);
			return false;
		}
	} else {
		spt_remove_page (spt, child);
		return false;
	}
	return true;
}
```

### 실패 시 부분 롤백

- **`vm_claim_page` 실패**: 그 VA에 넣어 둔 child page만 `spt_remove_page` (전체 copy는 아직 계속 시도할 수 있음 — 아래 `spt_copy_rollback`과 별개)
- **memcpy / swap 읽기 실패**: 이미 claim된 child page를 `spt_remove_page`
- **`supplemental_page_table_copy` 전체 실패**: `spt_copy_rollback`으로 자식 SPT **전체** 비움

---

## `spt_copy_rollback` — A와 공유

B를 넣으면서 A에 있던 `hash_destroy` + `hash_init` 반복을 함수로 묶었습니다.

```c
static void
spt_copy_rollback (struct supplemental_page_table *dst) {
	hash_destroy (&dst->hash, spt_page_destructor);
	hash_init (&dst->hash, page_hash, page_less, NULL);
}
```

UNINIT·ANON 복사 중 **어느 한 페이지라도 실패**하면, 지금까지 자식에 쌓인 **모든 page**를 destructor로 지우고 빈 hash로 되돌립니다. `__do_fork`는 `goto error`로 갑니다.

---

## `supplemental_page_table_copy` 통합 분기

```c
ty = VM_TYPE (src_page->operations->type);

if (ty == VM_UNINIT) {
	if (!spt_copy_uninit_page (src_page)) { ... }
} else if (ty == VM_ANON) {
	if (!spt_copy_loaded_anon_page (src_page)) { ... }
} else {
	/* Merge 5-C: file-backed / mmap */
	spt_copy_rollback (dst);
	return false;
}
```

**분기 키는 `operations->type`** 입니다. `page_get_type`이 아닙니다.

- exec lazy는 아직 `VM_UNINIT` → A
- materialize 후 `VM_ANON` → B
- `VM_FILE` → C (아직 `false`)

---

## fork 후 부모·자식 독립성

| 자원 | fork 후 |
|------|---------|
| `struct page` | 부모·자식 각각 malloc |
| `struct frame` | 각각 claim으로 별도 할당 |
| 물리 내용 | `memcpy` / `anon_read_swap_to_kva`로 **복제** |
| 부모 swap 슬롯 | B에서 **읽기만**, 해제 안 함 |
| 자식 swap 슬롯 | `ANON_SWAP_SLOT_NONE` (RAM에만 존재) |

이후 부모가 스택을 수정해도 자식 스택은 **다른 kva**를 가리키므로 서로 영향이 없습니다. (COW 없이도 읽기·실행은 독립 — 쓰기도 물리 page가 다르면 독립)

---

## A+B만으로 fork가 되는 경우 / 안 되는 경우

**되는 경우 (대략)**

- 부모 SPT가 **UNINIT + loaded anon** 만으로 구성
- 예: exec lazy page 전부 + 스택 bottom claim 한 페이지

**아직 안 되는 경우**

- `VM_FILE` exec segment가 이미 load된 경우, mmap page → **C 필요**
- `child-qsort` 등 file-backed 위주 fork 테스트

---

## 구현 순서 (실제 작업 순서)

1. **`anon_read_swap_to_kva`** — `anon.c` / `anon.h` (swap out 부모 대비)
2. **`spt_copy_loaded_anon_page`** — alloc → claim → 복사 → 실패 시 remove
3. **`spt_copy_rollback`** — A 실패 경로와 B 통합
4. **`supplemental_page_table_copy`** — `VM_ANON` 분기 추가

---

## 완료 체크리스트 (B)

- [ ] 부모 `VM_ANON` page가 자식에도 `VM_ANON` + `frame != NULL`로 존재한다.
- [ ] fork 직후 자식 anon 내용이 부모와 같다 (`memcmp` 한 페이지).
- [ ] 부모 `frame`이 있을 때 `memcpy` 경로를 탄다.
- [ ] 부모만 swap out일 때 `anon_read_swap_to_kva`를 쓰고, 부모 `swap_slot`은 유지된다.
- [ ] `vm_claim_page` 실패 시 child page leak이 없다.
- [ ] B 코드에 file/mmap aux 복사가 섞여 있지 않다.

---

## 수정 파일

| 파일 | 내용 |
|------|------|
| `pintos/vm/vm.c` | `spt_copy_loaded_anon_page`, `spt_copy_rollback`, `supplemental_page_table_copy` 분기 |
| `pintos/vm/anon.c` | `anon_read_swap_to_kva` |
| `pintos/include/vm/anon.h` | `anon_read_swap_to_kva` 선언 |
| (의존) Merge 5-A | `spt_copy_uninit_page` 등 |

---

## 다음 단계

```text
  A — uninit lazy 복사        ✅
  B — loaded anon 복사        ✅ (이 문서)
  C — file-backed / mmap     ← supplemental_page_table_copy VM_FILE 분기
  D — exit / kill 안정화
```

C가 붙어야 `load`로 깔린 exec page·`mmap` 구간이 있는 프로세스를 fork할 수 있습니다.

---

## 참고 — `anon_swap_in` vs `anon_read_swap_to_kva`

| | `anon_swap_in` | `anon_read_swap_to_kva` |
|---|----------------|-------------------------|
| 호출 시점 | page fault / claim | fork B |
| bitmap | `bitmap_reset` (슬롯 반환) | **변경 없음** |
| page->swap_slot | `NONE`으로 클리어 | **부모 page는 그대로** |
| 용도 | eviction 후 재접속 | 부모 스냅샷 → 자식 frame |

fork 경로에서 실수로 `anon_swap_in`을 부모 슬롯에 쓰면, **부모 page의 swap 데이터가 날아가는** 버그로 이어질 수 있어서 함수를 분리했습니다.
