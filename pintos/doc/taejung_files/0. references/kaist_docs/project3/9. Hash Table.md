Pintos는 `lib/kernel/hash.c`에 hash table 자료구조를 제공합니다. 이를 사용하려면 헤더 파일 '`lib/kernel/hash.h`'를 `#include <hash.h>`로 포함해야 합니다. Pintos가 제공하는 어떤 코드도 이 hash table을 사용하지 않으므로, 여러분은 원하는 대로 그대로 사용하거나, 자신의 목적에 맞게 구현을 수정하거나, 무시할 수 있습니다.

가상 메모리 프로젝트의 대부분 구현은 page를 frame으로 변환하기 위해 hash table을 사용합니다. 그 밖에도 hash table을 유용하게 쓸 수 있는 곳이 있을 것입니다.

## Data Types

hash table은 struct hash로 표현됩니다.

---

```
struct hash;
```

> 전체 hash table을 나타냅니다. `struct hash`의 실제 멤버는 "opaque"입니다. 즉 hash table을 사용하는 코드는 `struct hash` 멤버에 직접 접근해서는 안 되며, 그럴 필요도 없습니다. 대신 hash table 함수와 매크로를 사용하세요.

hash table은 `struct hash_elem` 타입의 element에 대해 동작합니다.

---

```
struct hash_elem;
```

> hash table에 넣고 싶은 구조체 안에 `struct hash_elem` 멤버를 포함하세요. `struct hash`와 마찬가지로 `struct hash_elem`도 opaque입니다. hash table element에 대해 동작하는 모든 함수는 실제 hash table element 타입의 pointer가 아니라 `struct hash_elem`에 대한 pointer를 받고 반환합니다.

실제 hash table element가 주어졌을 때 `struct hash_elem`을 얻거나, 반대로 `struct hash_elem`이 주어졌을 때 실제 element를 얻어야 하는 경우가 자주 있습니다. 실제 hash table element가 주어졌다면 `&` 연산자를 사용해 그 안의 `struct hash_elem`에 대한 pointer를 얻을 수 있습니다. 반대 방향으로 가려면 `hash_entry()` 매크로를 사용하세요.

---

```
#define hash_entry (elem, type, member) { /* 세부 내용 생략 */ }
```

> `struct hash_elem`에 대한 pointer인 elem이 포함되어 있는 구조체에 대한 pointer를 반환합니다. type에는 elem이 들어 있는 구조체의 이름을, member에는 type 안에서 elem이 가리키는 멤버의 이름을 제공해야 합니다.
> 
> 예를 들어 `h`가 `h_elem`이라는 이름의 struct thread 멤버(`struct hash_elem` 타입)를 가리키는 `struct hash_elem *` 변수라고 합시다. 그러면 `hash_entry`(`h, struct thread, h_elem`)은 `h`가 내부를 가리키고 있는 `struct thread`의 주소를 산출합니다.

각 hash table element는 key를 포함해야 합니다. key는 element를 식별하고 구별하는 데이터이며, hash table 안의 element들 사이에서 고유해야 합니다. element에는 key가 아닌 데이터도 포함될 수 있으며, 이 데이터는 고유할 필요가 없습니다. element가 hash table 안에 있는 동안에는 key data를 변경해서는 안 됩니다. 필요하다면 element를 hash table에서 제거하고, key를 수정한 다음 다시 삽입하세요.

각 hash table에 대해 key에 작용하는 두 함수를 작성해야 합니다. hash function과 comparison function입니다. 이 함수들은 다음 prototype과 일치해야 합니다.

---

```
typedef unsigned hash_hash_func (const struct hash_elem *e, void *aux);
```

> element의 데이터에 대한 hash를 unsigned int 범위 안의 값으로 반환합니다. element의 hash는 element의 key에 대한 pseudo-random function이어야 합니다. key가 아닌 데이터나 key 외의 비상수 데이터에 의존해서는 안 됩니다. Pintos는 hash function의 적절한 기반으로 사용할 수 있는 다음 함수들을 제공합니다.
> 
> `unsigned hash_bytes (const void *buf, size t *size)`
> 
> > buf에서 시작하는 size 바이트에 대한 hash를 반환합니다. 구현은 32비트 word용 범용 Fowler-Noll-Vo hash(`http://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash`)입니다.
> 
> `unsigned hash_string (const char *s)`
> 
> > null-terminated string s에 대한 hash를 반환합니다.
> 
> `unsigned hash_int (int i)`
> 
> > 정수 i에 대한 hash를 반환합니다.
> 
> key가 적절한 타입의 단일 데이터라면, hash function이 이 함수들 중 하나의 출력을 그대로 반환하는 것이 합리적입니다. 여러 데이터 조각이 key라면, 예를 들어 '^'(exclusive or) 연산자를 사용해 이 함수들을 여러 번 호출한 출력을 결합할 수 있습니다. 마지막으로 이 함수들을 완전히 무시하고 hash function을 처음부터 직접 작성할 수도 있지만, 목표는 hash function을 설계하는 것이 아니라 운영체제 커널을 만드는 것임을 기억하세요. aux에 대한 설명은 [Hash Auxiliary Data] 섹션을 참고하세요.
> 
> `bool hash_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux)`
> 
> > element a와 b에 저장된 key를 비교합니다. a가 b보다 작으면 true를 반환하고, a가 b보다 크거나 같으면 false를 반환합니다. 두 element가 같다고 비교된다면, 두 element는 같은 hash value로 hash되어야 합니다.
> 
> aux에 대한 설명은 [Hash Auxiliary Data] 섹션을 참고하세요. hash와 comparison function 예시는 [Hash Table Example] 섹션을 참고하세요. 몇몇 함수는 세 번째 종류의 함수에 대한 pointer를 인자로 받습니다.
> 
> `void hash_action_func (struct hash_elem *element, void *aux)`
> 
> > 호출자가 선택한 어떤 종류의 action을 element에 수행합니다. aux에 대한 설명은 [Hash Auxiliary Data] 섹션을 참고하세요.

## Basic Functions

이 함수들은 hash table을 생성하고, 파괴하고, 검사합니다.

---

```
bool hash_init (struct hash *hash, hash_hash_func *hash_func,
                    hash_less_func *less_func, void *aux)
```

> hash를 hash table로 초기화합니다. hash func는 hash function, less func는 comparison function, aux는 auxiliary data입니다. 성공하면 true, 실패하면 false를 반환합니다. `hash_init()`은 `malloc()`을 호출하므로 메모리를 할당할 수 없으면 실패합니다. aux에 대한 설명은 Hash Auxiliary Data를 참고하세요. aux는 보통 null pointer입니다.

---

```
void hash_clear (struct hash *hash, hash_action_func *action)
```

> hash에서 모든 element를 제거합니다. hash는 이전에 `hash_init()`으로 초기화되어 있어야 합니다. action이 null이 아니면, hash table의 각 element에 대해 한 번씩 호출됩니다. 이를 통해 호출자는 element가 사용하는 메모리나 다른 자원을 해제할 수 있습니다. 예를 들어 hash table element가 `malloc()`으로 동적 할당되었다면 action은 element를 `free()`할 수 있습니다. `hash_clear()`는 action을 호출한 뒤 해당 hash element의 메모리에 접근하지 않으므로 안전합니다. 하지만 action은 `hash_insert()`나 `hash_delete()`처럼 hash table을 수정할 수 있는 함수를 호출해서는 안 됩니다.

---

```
void hash_destroy (struct hash *hash, hash_action_func *action);
```

> action이 null이 아니면, `hash_clear()` 호출과 같은 의미로 hash의 각 element에 대해 action을 호출합니다. 그런 다음 hash가 보유한 메모리를 해제합니다. 이후에는 다시 hash_init()을 호출하기 전까지 hash를 어떤 hash table 함수에도 전달해서는 안 됩니다.

---

```
size_t hash_size (struct hash *hash);
```

> hash에 현재 저장된 element 수를 반환합니다.

---

```
bool hash_empty (struct hash *hash);
```

> hash가 현재 element를 하나도 포함하지 않으면 true를, 적어도 하나의 element를 포함하면 false를 반환합니다.

## Search Functions

각 함수는 hash table에서 제공된 element와 같다고 비교되는 element를 검색합니다. 검색 성공 여부에 따라 새 element를 hash table에 삽입하거나, 단순히 검색 결과를 반환하는 등의 action을 수행합니다.

---

```
struct hash_elem *hash_insert (struct hash *hash, struct hash elem *element);
```

> hash에서 element와 같은 element를 검색합니다. 없으면 element를 hash에 삽입하고 null pointer를 반환합니다. table에 이미 element와 같은 element가 있다면 hash를 수정하지 않고 그 element를 반환합니다.

---

```
struct hash_elem *hash_replace (struct hash *hash, struct hash elem *element);
```

> element를 hash에 삽입합니다. hash 안에 element와 같은 기존 element가 있으면 제거됩니다. 제거된 element를 반환하거나, hash에 element와 같은 element가 없었다면 null pointer를 반환합니다.
> 
> 반환된 element와 관련된 자원을 적절히 해제하는 것은 호출자의 책임입니다. 예를 들어 hash table element가 `malloc()`으로 동적 할당되었다면, 더 이상 필요하지 않을 때 호출자가 그 element를 `free()`해야 합니다.

다음 함수들에 전달되는 element는 hashing과 comparison 용도로만 사용됩니다. 실제로 hash table에 삽입되지는 않습니다. 따라서 element 안의 key data만 초기화하면 되며, 다른 데이터는 사용되지 않습니다. 보통 element type의 local variable을 선언하고 key data를 초기화한 뒤, 그 안의 `struct hash_elem` 주소를 `hash_find()`나 `hash_delete()`에 전달하는 방식이 자연스럽습니다. 예시는 Hash Table Example을 참고하세요. 큰 구조체는 local variable로 할당하지 않아야 합니다. 자세한 내용은 [struct thread](https://casys-kaist.github.io/pintos-kaist/appendix/threads.html)를 참고하세요.

---

```
struct hash_elem *hash_find (struct hash *hash, struct hash elem *element);
```

> hash에서 element와 같은 element를 검색합니다. 찾으면 찾은 element를 반환하고, 없으면 null pointer를 반환합니다.

---

```
struct hash_elem *hash_delete (struct hash *hash, struct hash elem *element);
```

> hash에서 element와 같은 element를 검색합니다. 찾으면 hash에서 제거하고 반환합니다. 찾지 못하면 null pointer를 반환하고 hash는 변경되지 않습니다.
> 
> 반환된 element와 관련된 자원을 적절히 해제하는 것은 호출자의 책임입니다. 예를 들어 hash table element가 `malloc()`으로 동적 할당되었다면, 더 이상 필요하지 않을 때 호출자가 그 element를 `free()`해야 합니다.

## Iteration Functions

이 함수들은 hash table 안의 element를 순회할 수 있게 합니다. 두 가지 인터페이스가 제공됩니다. 첫 번째는 각 element에 작용할 hash_action_func를 작성해 제공해야 합니다.

---

```
void hash_apply (struct hash *hash, hash action func *action);
```

> hash의 각 element에 대해 임의의 순서로 action을 한 번씩 호출합니다. action은 `hash_insert()`나 `hash_delete()`처럼 hash table을 수정할 수 있는 함수를 호출해서는 안 됩니다. action은 element의 key data를 수정해서는 안 되지만, 그 밖의 데이터는 수정할 수 있습니다.

두 번째 인터페이스는 "iterator" 자료형을 기반으로 합니다. 관용적으로 iterator는 다음처럼 사용합니다.

---

```
struct hash_iterator i;
hash_first (&i, h);
while (hash_next (&i)) {
    struct foo *f = hash_entry (hash_cur (&i), struct foo, elem);
    . . . f로 무언가 수행 . . .
}
```

---

```
struct hash_iterator;
```

> hash table 안의 위치를 나타냅니다. `hash_insert()`나 `hash_delete()`처럼 hash table을 수정할 수 있는 어떤 함수든 호출하면, 그 hash table의 모든 iterator가 무효화됩니다.
> 
> struct hash와 struct hash_elem처럼 struct hash_elem도 opaque입니다.

---

```
void hash_first (struct hash iterator *iterator, struct hash *hash);
```

> iterator를 hash의 첫 번째 element 바로 앞 위치로 초기화합니다.

```
struct hash_elem *hash_next (struct hash iterator *iterator);
```

> iterator를 hash의 다음 element로 전진시키고, 그 element를 반환합니다. 남은 element가 없으면 null pointer를 반환합니다. `hash_next()`가 iterator에 대해 null을 반환한 뒤 다시 호출하면 undefined behavior입니다.

---

```
struct hash_elem *hash_cur (struct hash iterator *iterator);
```

> iterator에 대해 `hash_next()`가 가장 최근에 반환한 값을 반환합니다. iterator에 `hash_first()`를 호출한 뒤 첫 번째 `hash_next()`를 호출하기 전에는 undefined behavior입니다.

## Hash Table Example

hash table에 넣고 싶은 `struct page`라는 구조체가 있다고 합시다. 먼저 `struct page` 안에 `struct hash_elem` 멤버를 포함하도록 정의합니다.

```
struct page {
    struct hash_elem hash_elem; /* Hash table element. */
    void *addr; /* 가상 주소. */
    /* . . . 다른 멤버들. . . */
};
```

addr을 key로 사용하는 hash function과 comparison function을 작성합니다. pointer는 그 byte를 기반으로 hash할 수 있고, pointer 비교에는 `<` 연산자가 잘 동작합니다.

```
/* page p에 대한 hash value를 반환합니다. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}
```

```
/* page a가 page b보다 앞서면 true를 반환합니다. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}
```

이 함수들의 prototype에서 `UNUSED`를 사용하면 aux가 사용되지 않는다는 warning을 억제합니다. `UNUSED`에 대한 정보는 Function and Parameter Attributes를 참고하세요. aux에 대한 설명은 Hash Auxiliary Data를 참고하세요.

그런 다음 다음처럼 hash table을 생성할 수 있습니다.

```
struct hash pages;
hash_init (&pages, page_hash, page_less, NULL);
```

이제 생성한 hash table을 조작할 수 있습니다. `p`가 `struct page`에 대한 pointer라면, 다음처럼 hash table에 삽입할 수 있습니다.

```
hash_insert (&pages, &p->hash_elem);
```

pages에 같은 addr을 가진 page가 이미 있을 가능성이 있다면, `hash_insert()`의 반환값을 확인해야 합니다.

hash table에서 element를 검색하려면 `hash_find()`를 사용합니다. `hash_find()`는 비교 대상 element를 받기 때문에 약간의 준비가 필요합니다. 다음은 pages가 file scope에 정의되어 있다고 가정하고, 가상 주소를 기반으로 page를 찾아 반환하는 함수입니다.

```
/* 주어진 가상 주소를 포함하는 page를 반환하고, 그런 page가 없으면 null pointer를 반환합니다. */
struct page *
page_lookup (const void *address) {
  struct page p;
  struct hash_elem *e;

  p.addr = address;
  e = hash_find (&pages, &p.hash_elem);
  return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}
```

여기서 `struct page`는 꽤 작다고 가정하고 local variable로 할당했습니다. 큰 구조체는 local variable로 할당하지 않아야 합니다.

비슷한 방식으로 `hash_delete()`를 사용해 주소로 page를 삭제하는 함수도 만들 수 있습니다.

## Auxiliary Data

위 예시처럼 단순한 경우에는 aux parameter가 필요 없습니다. 이런 경우에는 `hash_init()`에 aux로 null pointer를 넘기고, hash function과 comparison function에 전달되는 값을 무시하면 됩니다. aux parameter를 사용하지 않으면 compiler warning이 발생하지만, 예시처럼 `UNUSED` 매크로로 끄거나 그냥 무시할 수 있습니다.

**_aux_**는 hash table 안의 data item 자체에는 저장되어 있지 않지만, hashing이나 comparison에 필요하고 constant인 어떤 속성이 있을 때 유용합니다. 예를 들어 hash table의 item이 fixed-length string인데 item 자체가 그 fixed length를 나타내지 않는다면, length를 aux parameter로 전달할 수 있습니다.

## Synchronization

hash table은 내부 synchronization을 수행하지 않습니다. hash table 함수 호출을 동기화하는 것은 호출자의 책임입니다. 일반적으로 `hash_find()`나 `hash_next()`처럼 hash table을 검사하지만 수정하지 않는 함수는 여러 개가 동시에 실행될 수 있습니다. 그러나 이런 함수들은 `hash_insert()`나 `hash_delete()`처럼 주어진 hash table을 수정할 수 있는 함수와 동시에 안전하게 실행될 수 없습니다. 또한 주어진 hash table을 수정할 수 있는 함수가 둘 이상 동시에 실행되어도 안전하지 않습니다.

hash table element 안의 data에 대한 접근을 동기화하는 것도 호출자의 책임입니다. 이 data에 대한 접근을 어떻게 동기화할지는 다른 자료구조와 마찬가지로 그 data가 어떻게 설계되고 구성되었는지에 달려 있습니다.
