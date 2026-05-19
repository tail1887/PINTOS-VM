#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

#define ANON_SWAP_SLOT_NONE ((size_t) -1) // 스왑 슬롯이 없는 경우

struct anon_page {
    size_t swap_slot; // 스왑 슬롯 번호
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
