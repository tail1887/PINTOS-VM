`threads/mmu.c`의 코드는 x86_64 하드웨어 페이지 테이블에 대한 추상 인터페이스입니다. Pintos의 페이지 테이블, 즉 이 프로젝트에서 사용할 페이지 테이블은 Intel 프로세서 문서에서 Page-Map-Level-4를 뜻하는 `pml4`라고 부릅니다. 테이블이 4단계 구조이기 때문입니다. 페이지 테이블 인터페이스는 페이지 테이블을 표현하기 위해 `uint64_t *`를 사용합니다. 내부 구조에 접근하기 편리하기 때문입니다. 아래 섹션들은 페이지 테이블 인터페이스와 내부 구조를 설명합니다.

## 생성, 파괴, 활성화

이 함수들은 페이지 테이블을 생성하고, 파괴하고, 활성화합니다. 기본 Pintos 코드는 필요한 곳에서 이미 이 함수들을 호출하므로, 직접 호출할 필요는 없을 것입니다.

---

```
uint64_t * pml4_create (void);
```

> 새 페이지 테이블을 생성해 반환합니다. 새 페이지 테이블은 Pintos의 일반적인 커널 가상 페이지 mapping은 포함하지만, 사용자 가상 mapping은 포함하지 않습니다. 메모리를 얻을 수 없으면 null pointer를 반환합니다.

---

```
void pml4_destroy (uint64_t *pml4);
```

> 페이지 테이블 자체와 그것이 매핑하는 프레임을 포함해 pml4가 보유한 모든 자원을 해제합니다. 페이지 테이블의 모든 단계에서 자원을 해제하기 위해 `pdpe_destroy`, `pgdir_destory`, `pt_destroy`를 재귀적으로 호출합니다.

---

```
void pml4_activate (uint64 t *pml4)
```

> pml4를 활성화합니다. 활성 페이지 테이블은 CPU가 메모리 참조를 변환할 때 사용하는 페이지 테이블입니다.

## 검사와 갱신

이 함수들은 페이지 테이블이 캡슐화한 page-to-frame mapping을 검사하거나 갱신합니다. 실행 중인 프로세스와 중단된 프로세스의 페이지 테이블, 즉 active와 inactive 페이지 테이블 모두에서 동작하며, 필요하면 TLB를 flush합니다.

---

```
bool pml4_set_page (uint64_t *pml4, void *upage, void *kpage, bool rw);
```

> 사용자 페이지 upage에서 커널 가상 주소 kpage로 식별되는 프레임으로의 mapping을 pd에 추가합니다. rw가 true이면 페이지는 read/write로 매핑되고, 그렇지 않으면 read-only로 매핑됩니다. 사용자 페이지 upage는 pml4에 이미 매핑되어 있으면 안 됩니다. 커널 페이지 kpage는 `palloc_get_page(PAL_USER)`로 사용자 풀에서 얻은 커널 가상 주소여야 합니다. 성공하면 true, 실패하면 false를 반환합니다. 페이지 테이블에 필요한 추가 메모리를 얻을 수 없으면 실패합니다.

---

```
void * pml4_get_page (uint64_t *pml4, const void *uaddr);
```

> pml4에서 uaddr에 매핑된 프레임을 찾습니다. uaddr이 매핑되어 있으면 해당 프레임의 커널 가상 주소를 반환하고, 그렇지 않으면 null pointer를 반환합니다.

---

```
void pml4_clear_page (uint64_t *pml4, void *upage);
```

> pml4에서 페이지를 "not present"로 표시합니다. 이후 해당 페이지에 접근하면 fault가 발생합니다. 페이지 테이블의 다른 bit들은 보존되므로, accessed bit와 dirty bit를 나중에 확인할 수 있습니다. 페이지가 매핑되어 있지 않으면 이 함수는 아무 효과가 없습니다.

## Accessed Bit와 Dirty Bit

x86_64 하드웨어는 각 페이지의 page table entry(PTE)에 있는 두 bit를 통해 페이지 교체 알고리즘 구현을 일부 도와줍니다. 페이지를 읽거나 쓰면 CPU는 그 페이지의 PTE에서 accessed bit를 1로 설정하고, 쓰기가 발생하면 dirty bit를 1로 설정합니다. CPU는 이 bit들을 0으로 초기화하지 않지만, OS는 그렇게 할 수 있습니다. 이 bit들을 올바르게 해석하려면 _aliases_, 즉 같은 프레임을 참조하는 두 개 이상의 페이지를 이해해야 합니다. aliased frame에 접근하면 accessed bit와 dirty bit는 접근에 사용된 페이지의 page table entry 하나에서만 갱신됩니다. 다른 alias들의 accessed bit와 dirty bit는 갱신되지 않습니다.

---

```
bool pml4_is_dirty (uint64_t *pml4, const void *vpage);
bool pml4_is_accessed (uint64_t *pml4, const void *vpage);
```

> pml4가 vpage에 대해 dirty 또는 accessed로 표시된 page table entry를 포함하면 true를 반환합니다. 그렇지 않으면 false를 반환합니다.

---

```
void pml4_set_dirty (uint64_t *pml4, const void *vpage, bool dirty);
void pml4_set_accessed (uint64_t *pml4, const void *vpage, bool accessed);
```

> pml4가 page에 대한 page table entry를 가지고 있다면, dirty 또는 accessed bit를 주어진 값으로 설정합니다.
