# 03 — 기능 2: SPT Insert/Find/Remove

## 1. 구현 목적 및 필요성

### 이 기능이 무엇인가
SPT에 page를 등록하고, fault address로 page를 찾고, cleanup 시 제거하는 기본 조작입니다.

### 왜 이걸 하는가
VM의 모든 기능은 "이 주소가 어떤 page인가"를 찾는 데서 시작합니다.

### 무엇을 연결하는가
`vm_alloc_page_with_initializer()`, `spt_find_page()`, `spt_insert_page()`, `vm_try_handle_fault()`를 연결합니다.

### 완성의 의미
합법적인 lazy page는 fault에서 발견되고, 이미 사용 중인 주소는 중복 등록되지 않습니다.

## 2. 가능한 구현 방식 비교

- 방식 A: SPT helper 내부에서 주소 align
  - 장점: caller 실수를 줄임
  - 단점: page 생성 시 align 규칙도 함께 관리해야 함
- 방식 B: caller가 항상 align
  - 장점: helper 단순
  - 단점: 호출 지점이 늘면 실수 위험
- 선택: helper에서도 align하고, page 생성 지점에서도 align을 보장한다.

## 3. 시퀀스와 단계별 흐름

```mermaid
sequenceDiagram
  participant L as process load/mmap/stack
  participant S as SPT
  participant F as page_fault

  L->>S: spt_insert_page(page)
  F->>S: spt_find_page(fault_addr)
  S-->>F: page or NULL
```

1. load/mmap/stack growth가 page metadata를 만든다.
2. SPT insert가 중복 va를 검사한다.
3. fault path가 page-aligned 주소로 find한다.
4. 실패하면 stack growth 또는 kill 판단으로 넘어간다.

## 4. 기능별 가이드 (개념/흐름 + 구현 주석 위치)

### 4.1 기능 A: SPT page 등록
#### 개념 설명
SPT insert는 새 page metadata를 현재 process 주소 공간에 등록하는 단계입니다. 같은 upage가 이미 있으면 기존 page를 덮어쓰지 말고 실패해야 이후 fault 처리와 cleanup 경로가 예측 가능합니다.

#### 시퀀스 및 흐름
```mermaid
flowchart LR
  PAGE[new page] --> KEY[page->va 확인]
  KEY --> DUP{이미 존재?}
  DUP -- 예 --> FAIL[false 반환]
  DUP -- 아니오 --> INSERT[hash_insert]
```

1. `page->va`가 page-aligned 상태인지 확인한다.
2. hash table에 같은 key가 있는지 insert 결과로 판단한다.
3. 실패하면 caller가 page와 aux를 정리할 수 있도록 false를 반환한다.

#### 구현 주석 (보면 되는 함수/구조체)
- 위치: `vm/vm.c`의 `spt_insert_page()`
- 위치: `include/vm/vm.h`의 `struct supplemental_page_table`

### 4.2 기능 B: fault address 기반 조회
#### 개념 설명
page fault는 page 내부의 임의 byte 주소에서 발생할 수 있습니다. SPT lookup은 이 주소를 page boundary로 내린 뒤 같은 page entry를 찾아야 합니다.

#### 시퀀스 및 흐름
```mermaid
flowchart TD
  FAULT[fault address] --> ROUND[pg_round_down]
  ROUND --> TEMP[temp page key]
  TEMP --> FIND[hash_find]
  FIND --> RESULT[page or NULL]
```

1. 조회 대상 주소를 `pg_round_down()`으로 정규화한다.
2. hash table이 이해할 수 있는 임시 key를 만든다.
3. entry가 없으면 NULL을 반환해 stack growth 또는 kill 판단으로 넘긴다.

#### 구현 주석 (보면 되는 함수/구조체)
- 위치: `vm/vm.c`의 `spt_find_page()`
- 위치: `threads/vaddr.h`의 `pg_round_down()`

### 4.3 기능 C: page 제거와 destroy 경계
#### 개념 설명
SPT remove는 hash entry 제거와 page 자원 해제를 연결하는 경계입니다. page type별 cleanup은 destroy hook에서 처리하고, remove는 같은 page가 두 번 정리되지 않도록 호출 순서를 명확히 해야 합니다.

#### 시퀀스 및 흐름
```mermaid
flowchart LR
  REMOVE[spt_remove_page] --> DELETE[hash_delete]
  DELETE --> DESTROY[vm_dealloc_page]
  DESTROY --> HOOK[type별 destroy]
```

1. 제거 대상 page가 실제 SPT에 들어 있는지 확인한다.
2. hash entry를 제거한 뒤 page destroy 경로를 호출한다.
3. munmap, process exit, fork 실패 cleanup이 같은 page를 중복 제거하지 않게 한다.

#### 구현 주석 (보면 되는 함수/구조체)
- 위치: `vm/vm.c`의 `spt_remove_page()`
- 위치: `vm/vm.c`의 `vm_dealloc_page()`

## 5. 구현 주석

### 5.1 `spt_insert_page()`

#### 5.1.1 `spt_insert_page()`에서 `page->va`를 hash table에 등록
- 수정 위치: `vm/vm.c`의 `spt_insert_page()`
- 역할: 현재 process SPT에 새 page를 등록한다.
- 규칙 1: `page->va`가 이미 있으면 실패한다.
- 규칙 2: 성공/실패를 caller가 확인할 수 있게 bool로 반환한다.
- 금지 1: 중복 entry를 덮어쓰지 않는다.

구현 체크 순서:
1. `page->va` align 상태를 확인한다.
2. hash insert의 이전 element 반환을 확인한다.
3. insert 실패 시 page/aux 정리 책임을 caller와 맞춘다.

### 5.2 `spt_find_page()`

#### 5.2.1 `spt_find_page()`에서 fault address를 page key로 조회
- 수정 위치: `vm/vm.c`의 `spt_find_page()`
- 역할: 임의의 user va를 SPT key로 정규화해 page를 찾는다.
- 규칙 1: `pg_round_down(va)`로 key를 만든다.
- 규칙 2: 없으면 NULL을 반환한다.
- 금지 1: NULL 반환을 곧바로 kernel panic으로 처리하지 않는다.

구현 체크 순서:
1. stack 위/아래 주소로 lookup해 page align 동작을 확인한다.
2. SPT에 없는 주소에서 NULL이 나오는지 확인한다.
3. caller가 NULL을 stack growth 또는 kill로 분기하는지 확인한다.

## 6. 테스팅 방법

- lazy load 관련 단일 테스트
- mmap overlap 테스트
- stack growth가 SPT에 새 page를 넣는 경로
