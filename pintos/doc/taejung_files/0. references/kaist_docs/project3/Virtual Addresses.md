64비트 가상 주소는 다음과 같이 구성됩니다.

```
63          48 47            39 38            30 29            21 20         12 11         0
+-------------+----------------+----------------+----------------+-------------+------------+
| Sign Extend |    Page-Map    | Page-Directory | Page-directory |  Page-Table |  Physical  |
|             | Level-4 Offset |    Pointer     |     Offset     |   Offset    |   Offset   |
+-------------+----------------+----------------+----------------+-------------+------------+
              |                |                |                |             |            |
              +------- 9 ------+------- 9 ------+------- 9 ------+----- 9 -----+---- 12 ----+
                                          Virtual Address
```

헤더 `include/threads/vaddr.h`와 `include/threads/mmu.h`는 가상 주소를 다루기 위한 다음 함수와 매크로를 정의합니다.

---

```
#define PGSHIFT { /* 세부 내용 생략 */ }
#define PGBITS { /* 세부 내용 생략 */ }
```

> 가상 주소의 offset 부분에 대한 bit index(0)와 bit 수(12)입니다.

---

```
#define PGMASK { /* 세부 내용 생략 */ }
```

> page offset에 해당하는 bit들은 1로, 나머지는 0으로 설정된 bit mask입니다. 값은 0xfff입니다.

---

```
#define PGSIZE { /* 세부 내용 생략 */ }
```

> 페이지 크기입니다. 바이트 단위로 4,096입니다.

---

```
#define pg_ofs(va) { /* 세부 내용 생략 */ }
```

> 가상 주소 va에서 page offset을 추출해 반환합니다.

---

```
#define pg_no(va) { /* 세부 내용 생략 */ }
```

> 가상 주소 va에서 page number를 추출해 반환합니다.

---

```
#define pg_round_down(va) { /* 세부 내용 생략 */ }
```

> va가 가리키는 가상 페이지의 시작 주소를 반환합니다. 즉 page offset을 0으로 만든 va입니다.

---

```
#define pg_round_up(va) { /* 세부 내용 생략 */ }
```

> va를 가장 가까운 다음 page boundary로 올림한 값을 반환합니다.

Pintos의 가상 메모리는 user virtual memory와 kernel virtual memory라는 두 영역으로 나뉩니다. [Virtual Memory Layout](https://casys-kaist.github.io/pintos-kaist/project2/introduction)을 참고하세요.

두 영역의 경계는 `KERN_BASE`입니다.

---

```
#define KERN_BASE { /* 세부 내용 생략 */ }
```

> 커널 가상 메모리의 기준 주소입니다. 기본값은 0x8004000000입니다. 사용자 가상 메모리는 가상 주소 0부터 `KERN_BASE` 직전까지이고, 커널 가상 메모리는 나머지 가상 주소 공간을 차지합니다.

---

```
#define is_user_vaddr(vaddr) { /* 세부 내용 생략 */ }
#define is_kernel_vaddr(vaddr) { /* 세부 내용 생략 */ }
```

> va가 각각 user virtual address 또는 kernel virtual address이면 true를 반환하고, 그렇지 않으면 false를 반환합니다.

---

x86-64는 물리 주소가 주어졌을 때 그 메모리에 직접 접근하는 방법을 제공하지 않습니다. 운영체제 커널에서는 이 능력이 자주 필요하므로, Pintos는 커널 가상 메모리를 물리 메모리에 일대일로 매핑하여 이를 우회합니다. 즉 가상 주소 `KERN_BASE`는 물리 주소 0에 접근하고, 가상 주소 `KERN_BASE + 0x1234`는 물리 주소 0x1234에 접근하며, 이런 식으로 머신의 물리 메모리 크기까지 이어집니다. 따라서 물리 주소에 `KERN_BASE`를 더하면 그 주소에 접근하는 커널 가상 주소를 얻을 수 있습니다. 반대로 커널 가상 주소에서 `KERN_BASE`를 빼면 대응하는 물리 주소를 얻을 수 있습니다.

헤더 `include/threads/vaddr.h`는 이런 변환을 위한 함수 쌍을 제공합니다.

---

```
#define ptov(paddr) { /* 세부 내용 생략 */ }
```

> 물리 주소 pa에 대응하는 커널 가상 주소를 반환합니다. pa는 0과 물리 메모리 바이트 수 사이에 있어야 합니다.

---

```
#define vtop(vaddr) { /* 세부 내용 생략 */ }
```

> va에 대응하는 물리 주소를 반환합니다. va는 커널 가상 주소여야 합니다.

헤더 `include/threads/mmu.h`는 페이지 테이블에 대한 operation을 제공합니다.

---

```
#define is_user_pte(pte) { /* 세부 내용 생략 */ }
#define is_kern_pte(pte) { /* 세부 내용 생략 */ }
```

> page table entry(PTE)가 각각 user 또는 kernel 소유인지 질의합니다.

---

```
#define is_writable(pte) { /* 세부 내용 생략 */ }
```

> page table entry(PTE)가 가리키는 가상 주소가 writable인지 질의합니다.

---

```
typedef bool pte_for_each_func (uint64_t *pte, void *va, void *aux);
bool pml4_for_each (uint64_t *pml4, pte_for_each_func *func, void *aux);
```

> PML4 아래의 각 valid entry에 대해 aux 값을 함께 넘겨 FUNC를 적용합니다. VA는 해당 entry의 가상 주소를 나타냅니다. pte_for_each_func가 false를 반환하면 반복을 멈추고 false를 반환합니다.

아래는 `pml4_for_each`에 전달할 수 있는 `func`의 예입니다.

```
static bool
stat_page (uint64_t *pte, void *va,  void *aux) {
        if (is_user_vaddr (va))
                printf ("user page: %llx\n", va);
        if (is_writable (va))
                printf ("writable page: %llx\n", va);
        return true;
}
```
