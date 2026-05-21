# 머지 5 A 정리 — SPT Copy: Uninit Page

자, 이번 머지 5 A에서는 **fork 시 부모 SPT에 있는 lazy(uninit) page를 자식 SPT에 같은 의미로 복사**하는 데 있습니다.

VM에서는 유저 주소 공간 정보가 `pml4`뿐 아니라 **SPT(Supplemental Page Table)** 에도 들어 있습니다. 프로젝트 2에서는 `pml4_for_each`로 PTE를 통째로 복사했지만, VM에서는 “아직 물리 frame이 없는 lazy page”가 SPT에만 존재할 수 있죠. 그래서 fork 경로에서 **`supplemental_page_table_copy(&child->spt, &parent->spt)`** 가 반드시 필요합니다.

구체적으로 말하면 `process_fork` → `__do_fork` 안에서 자식 `pml4`를 만든 뒤, 아래 순서로 들어갑니다.

```c
supplemental_page_table_init (&current->spt);
if (!supplemental_page_table_copy (&current->spt, &parent->spt))
	goto error;
```

이때 **`thread_current()`는 이미 자식**입니다. `vm_alloc_page_with_initializer`가 내부에서 `&thread_current()->spt`에 넣기 때문에, `copy`의 `dst` 인자와 자식 SPT가 같은 대상이어야 합니다.

---

## A가 맡는 것 / 맡지 않는 것

부모 SPT의 page는 대략 세 가지 상태로 나뉩니다.

| 상태 | `operations->type` | A에서 할 일 |
|------|-------------------|-------------|
| 아직 fault 안 난 lazy page | `VM_UNINIT` | **복사 (A)** |
| 이미 frame에 올라간 anon | `VM_ANON` | **B에서 처리** |
| 이미 materialize된 file/mmap | `VM_FILE` | **C에서 처리** |

A의 핵심 계약은 문서에도 나와 있듯이 다음과 같습니다.

- 부모 `struct page` **포인터를 그대로 자식에 넣지 않는다** (새 page를 만든다).
- **`frame` 내용·`swap` 슬롯은 복사하지 않는다** (나중에 fault 때 claim).
- **`va`, `writable`, 목표 타입, `init`, `aux` 의미**는 유지한다.

즉 “메모리 내용 복사”가 아니라 **“lazy로 다시 등록해 두기”** 입니다.

---

## 전체 호출 흐름

```text
supplemental_page_table_copy(dst, src)
  └─ hash 순회 (부모 SPT)
       └─ operations->type == VM_UNINIT ?
            ├─ 아니오 → false + 자식 SPT 롤백 (B/C 미구현)
            └─ 예 → spt_copy_uninit_page(src_page)
                     ├─ spt_copy_uninit_aux(src)   // aux 깊은 복사
                     └─ vm_alloc_page_with_initializer(...)  // Merge 1 재사용
```

`page_get_type(parent)`는 UNINIT page일 때 **`uninit.type`**(최종 목표: `VM_ANON` / `VM_FILE`)을 돌려줍니다. 분기 조건은 **`operations->type == VM_UNINIT`** 로 봅니다. (이미 anon/file로 바뀐 page는 여기서 걸러짐)

---

## 왜 aux를 깊은 복사해야 할까?

`uninit_page`는 Merge 1에서 lazy 정보를 들고 있습니다.

```c
struct uninit_page {
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};
```

- **exec lazy segment**: `aux` = `segment_aux`가 들어 있는 **한 페이지** (`create_segment_aux`에서 `palloc`)
- **mmap lazy**: `aux` = `struct file_page` 메타데이터가 들어 있는 **한 페이지** (`do_mmap`에서 `palloc`)
- **anon stack growth 등**: `aux == NULL`, `init == NULL` 인 경우도 있음

부모와 자식이 **`aux` 페이지 포인터를 공유하면 안 됩니다.** 한쪽이 `uninit_destroy`에서 `palloc_free_page(aux)`를 호출하면 다른 쪽이 깨지기 때문입니다. 그래서 팀 규약은 **깊은 복사 + file은 `file_reopen`** 입니다.

---

## 구현 순서

문서 §2와 같이, 함수를 나눠서 넣는 순서가 이해하기 쉽습니다.

1. **`spt_page_destructor`** — copy 실패 시 자식 SPT 롤백용 (이미 kill에서 쓰던 것과 동일)
2. **`spt_copy_uninit_aux`** — aux 페이지 복사 + `VM_FILE`이면 `file_reopen`
3. **`spt_copy_uninit_page`** — 한 엔트리를 `vm_alloc_page_with_initializer`로 등록
4. **`supplemental_page_table_copy`** — 부모 hash 순회 + UNINIT만 위 함수 호출

---

## 1. `spt_copy_uninit_aux` — aux 깊은 복사

`aux`가 없으면 그대로 `NULL`을 반환합니다. anon lazy 중 `init`/`aux` 없는 케이스죠.

있으면 **새 커널 페이지 하나**를 `PAL_ZERO`로 받아서 부모 aux 내용을 `memcpy` 합니다. `PAL_ZERO`를 쓰는 이유는 exec `segment_aux`에는 `page_cnt` 같은 필드가 없는데, 나중에 `file_backed_initializer`가 `file_page`처럼 뒤 필드를 읽을 때 **쓰레기 값 방지** (머지 4 F 문서와 같은 맥락)입니다.

`page_get_type(src) == VM_FILE`이면 선두 필드가 `struct file *`이므로 **`file_reopen`** 으로 자식 전용 file 객체를 붙입니다. mmap/exec 모두 aux 레이아웃 앞부분이 `file_page`와 같아서 한 분기로 처리할 수 있습니다.

```c
static void *
spt_copy_uninit_aux (struct page *src) {
	struct uninit_page *u = &src->uninit;

	if (u->aux == NULL)
		return NULL;

	void *child_aux = palloc_get_page (PAL_ZERO);
	if (child_aux == NULL)
		return NULL;

	memcpy (child_aux, u->aux, PGSIZE);

	if (VM_TYPE (page_get_type (src)) == VM_FILE) {
		struct file_page *a = child_aux;
		if (a->file != NULL) {
			struct file *reopened = file_reopen (a->file);
			if (reopened == NULL) {
				palloc_free_page (child_aux);
				return NULL;
			}
			a->file = reopened;
		}
	}
	return child_aux;
}
```

`file_reopen` 실패 시 방금 잡은 aux 페이지는 바로 `palloc_free_page`로 되돌립니다.

---

## 2. `spt_copy_uninit_page` — 자식 SPT에 lazy 등록

여기서 **Merge 1 B 경로를 그대로 재사용**합니다. 직접 `uninit_new` + `spt_insert_page`를 반복 구현하지 않아도 됩니다.

```c
static bool
spt_copy_uninit_page (struct page *src) {
	struct uninit_page *u = &src->uninit;
	void *child_aux = spt_copy_uninit_aux (src);

	if (u->aux != NULL && child_aux == NULL)
		return false;

	if (!vm_alloc_page_with_initializer (page_get_type (src), src->va,
			src->writable, u->init, child_aux)) {
		if (child_aux != NULL)
			palloc_free_page (child_aux);
		return false;
	}
	return true;
}
```

주의할 점 두 가지입니다.

1. **`vm_alloc_page_with_initializer`가 성공하면** `child_aux` 소유권은 자식 `uninit_page`로 넘어갑니다. 실패했을 때만 위에서 `palloc_free_page(child_aux)` 합니다.
2. **frame은 할당하지 않습니다.** 첫 fault → `vm_do_claim_page` → `swap_in` → `uninit_initialize` → `init(page, aux)` 순서는 부모와 동일합니다.

`init`이 `lazy_load_segment` / `mmap_lazy_load` 같은 **함수 포인터**인데, 이건 코드 영역을 가리키므로 부모·자식이 **같은 주소를 공유해도 됩니다.** 복사 대상은 `aux` 데이터뿐입니다.

---

## 3. `supplemental_page_table_copy` — 순회와 롤백

부모 SPT를 hash로 순회하면서 UNINIT만 복사합니다.

```c
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	struct hash_elem *e;

	for (e = hash_begin (&src->hash); e != hash_end (&src->hash);
			e = hash_next (e)) {
		struct page *src_page = hash_entry (e, struct page, elem);

		if (VM_TYPE (src_page->operations->type) != VM_UNINIT) {
			hash_destroy (&dst->hash, spt_page_destructor);
			hash_init (&dst->hash, page_hash, page_less, NULL);
			return false;
		}

		if (!spt_copy_uninit_page (src_page)) {
			hash_destroy (&dst->hash, spt_page_destructor);
			hash_init (&dst->hash, page_hash, page_less, NULL);
			return false;
		}
	}
	return true;
}
```

### loaded page를 만나면 왜 `false`인가?

A만 구현한 단계에서는 **이미 claim된 page**(예: `setup_stack` 직후 스택 bottom)를 복사하지 못합니다. 그때 `false`를 반환하고, 이미 자식에 넣어 둔 UNINIT 엔트리는 **`spt_page_destructor`로 전부 지웁니다.** 그다음 빈 hash로 `hash_init` 다시 해서 `__do_fork`가 `error`로 빠지게 합니다.

이건 “일부만 복사된 채 fork 성공”보다 안전한 쪽입니다. **B(loaded anon)·C(file/mmap)** 가 붙으면 이 분기에서 `continue` 또는 타입별 copy로 바꿉니다.

### `__do_fork`와의 관계

`supplemental_page_table_init`으로 자식 SPT는 이미 빈 hash입니다. `copy` 실패 시 `hash_destroy` → `hash_init`으로 **다시 빈 테이블**로 맞춰 두면, `goto error` 이후 자식 정리 경로와 충돌이 적습니다.

---

## 부모 page와 자식 page 수명

| 자원 | fork 후 |
|------|---------|
| 부모 `struct page` | 부모 SPT가 소유, 그대로 |
| 자식 `struct page` | `vm_alloc_page_with_initializer`가 **새로 malloc** |
| 부모 `aux` 페이지 | 부모 `uninit_destroy` / lazy_load 완료 시 해제 |
| 자식 `aux` 페이지 | 자식 page 전용, `uninit_destroy`에서 해제 |

부모가 나중에 exec segment를 fault로 materialize하면 `lazy_load_segment`가 **부모 aux만** `free_segment_aux` 합니다. 자식 aux는 건드리지 않습니다.

---

## A만으로 fork가 통과하지 않는 이유 (다음 단계)

`load` / `setup_stack` 직후에는 스택 bottom 등 **이미 `VM_ANON`으로 바뀐 page**가 SPT에 있을 수 있습니다. A는 UNINIT만 복사하므로 `supplemental_page_table_copy`가 **`false`** 를 반환하고, `exec-wait-child` 같은 fork 테스트는 **머지 5 B** 이후에야 의미 있게 통과합니다.

```text
다음 구현 순서 (팀 마스터플랜)
  A — uninit lazy 복사     ← 이 문서
  B — loaded anon (frame 내용 memcpy + claim)
  C — file-backed / mmap 메타 복사
  D — exit / kill / 이중 free 방지 정리
```

---

## 완료 체크리스트 (A)

- [ ] 부모 UNINIT page가 자식 SPT에도 `operations->type == VM_UNINIT`으로 존재한다.
- [ ] copy 과정에서 `vm_get_frame` / `palloc(PAL_USER)`가 호출되지 않는다 (frame 즉시 할당 없음).
- [ ] exec/mmap aux는 **별도 페이지 + `file_reopen`** 으로 자식 전용이다.
- [ ] `vm_alloc_page_with_initializer` 실패 시 `child_aux` leak이 없다.
- [ ] loaded page가 섞인 fork는 B/C 전까지 `copy == false` (의도된 상태).

---

## 수정 파일

| 파일 | 내용 |
|------|------|
| `pintos/vm/vm.c` | `spt_copy_uninit_aux`, `spt_copy_uninit_page`, `supplemental_page_table_copy` |
| (의존) `pintos/vm/uninit.c` | 변경 없음 — `uninit_destroy`가 aux 페이지 해제 |
| (의존) `pintos/userprog/process.c` | `__do_fork`에서 `supplemental_page_table_copy` 호출 |

---

## 참고 — 잘못된 초안에서 피한 것

중간에 있던 초안은 대략 이런 문제가 있었습니다.

- `supplemental_page_table_copy` 반환형이 `void`이고, 실패해도 `true`처럼 진행
- `src_page->aux` — `struct page`에는 `aux` 필드가 없음 (`uninit.aux`여야 함)
- `uninit_new(..., src_page->operations->type, ...)` — type/init/aux/page_initializer 전부 틀림
- `spt_insert_page` 성공 시 `return`으로 **루프가 한 페이지만 복사하고 종료**

A 구현은 **`vm_alloc_page_with_initializer` 재사용 + aux 깊은 복사 + hash 전체 순회 + 실패 롤백** 으로 정리했습니다.
