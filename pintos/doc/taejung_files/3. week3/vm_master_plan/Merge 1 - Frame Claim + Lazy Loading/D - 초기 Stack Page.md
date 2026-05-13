# D – 초기 Stack Page

## 1. 개요 (목표·이유·수정 위치·의존성)

```text
목표
- 첫 stack page를 만들고 argument passing이 정상 동작하도록 연결한다.

이유
- 유저 프로그램 시작 시 stack은 즉시 필요하며, Project 2 argument passing과 직접 연결된다.

수정/추가 위치
- userprog/process.c
  - setup_stack()
  - argument_stack 관련 흐름 확인
- 필요 시 vm/vm.c
  - vm_claim_page() 사용 흐름 확인

의존성
- A의 vm_claim_page/vm_do_claim_page가 필요하다.
- B의 anon initializer가 있어야 stack page를 anonymous page로 다룰 수 있다.
```

## 2. 시퀀스

`setup_stack`이 **첫 스택 page를 SPT에 올린 뒤**, 구현에 따라 즉시 `vm_claim_page`로 올리거나 fault 때 **`00-서론.md` §1.1** 경로로 합류한다.

```mermaid
sequenceDiagram
	autonumber
	participant ST as setup_stack
	participant Alloc as vm_alloc_page_or_initializer
	participant S as spt_insert_page
	participant Cl as vm_claim_page
	participant Do as vm_do_claim_page

	ST->>Alloc: stack용 anon/uninit 등록
	Alloc->>S: SPT 삽입
	alt 즉시 claim 정책
		ST->>Cl: vm_claim_page(stack_va)
		Cl->>Do: vm_do_claim_page
	else 첫 접근 fault 정책
		ST-->>ST: 나중에 00-서론 §1.1 경로
	end
```

## 3. 단계별 설명 (이 문서 범위)

1. **`setup_stack`**: user stack 상단 근처 VA에 맞는 **한 page**를 VM 서브시스템에 등록한다.
2. **B 연동**: stack은 보통 **anonymous** 취급이므로 **`B - Uninit Page와 Initializer.md`** 의 anon initializer 경로와 맞춘다.
3. **A 연동**: 등록 직후 `vm_claim_page`로 frame을 붙이면 **`A - Frame Claim.md`** 와 동일 claim 몸체를 재사용한다. lazy로 두면 **`00-서론.md` §1.1** fault 경로로 첫 접근 시 올린다.
4. **Project 2**: `argument_stack` 등은 이 stack page가 유효한 뒤에 쌓인다.

## 4. 구현 주석 가이드

### 4.1 구현 대상 함수 목록

- `setup_stack` (`userprog/process.c`)

### 4.2 공통 구조체/필드 계약

- 스택 시작 VA는 `stack_bottom = USER_STACK - PGSIZE`로 고정한다.
- 최초 스택 페이지 타입은 `VM_ANON`, writable `true`로 둔다.
- 성공 시 `if_->rsp = USER_STACK`를 설정한다.
- 동적 스택 확장(`vm_stack_growth`)은 Merge 2에서만 다룬다.

### 4.3 함수별 구현 주석 (고정안)

D는 **스택 1페이지 등록 + (선택) 즉시 claim + rsp 설정**으로 고정한다.

#### §4.3.0 (이 문서)

[Merge 1 `00-서론.md`](00-%EC%84%9C%EB%A1%A0.md) §4.3.0과 동일.

---

#### `setup_stack` (`userprog/process.c`, `#ifdef VM`)

Merge 1–D에서 이 함수는 **`stack_bottom`에 스택용 페이지를 SPT에 넣고**, 팀 정책에 따라 **즉시 `vm_claim_page`로 올리거나** 첫 fault에 맡긴다. 성공 시 **`if_->rsp`를 `USER_STACK`으로** 맞춘다. `vm_stack_growth`는 Merge 2.

**흐름**

1. `void *stack_bottom = (uint8_t *) USER_STACK - PGSIZE;`
2. `vm_alloc_page_with_initializer(VM_ANON, stack_bottom, true, …)` 등으로 SPT 등록 — `uninit_new`/`anon_initializer` 조합은 B와 동일하게 맞춘다.
3. **즉시 매핑 정책**이면 `vm_claim_page(stack_bottom)` → 내부에서 `spt_find_page` → `vm_do_claim_page`. 아니면 SPT만 두고 첫 접근은 `00-서론` §1.1 fault 경로.
4. 성공 시 `if_->rsp = (uint64_t) USER_STACK` — 인자 스택 쌓기 전제.
5. **하지 않음 (D 경계)**: `vm_stack_growth`, user `rsp` 커널 저장 필드, fault 시 스택 범위 판별(Merge 2).
6. 이후 `push_arguments` 등은 매핑이 살아 있는 상태에서만 호출한다.

**플로우차트**

```mermaid
flowchart TD
  A([setup_stack]) --> B[stack_bottom 계산]
  B --> C{vm_alloc 성공?}
  C -->|아니오| Z([return false])
  C -->|예| D{즉시 claim 정책?}
  D -->|예| E{vm_claim_page 성공?}
  E -->|아니오| Z
  E -->|예| F[rsp USER_STACK]
  D -->|아니오| F
  F --> G([return true])
```

### 4.4 함수 간 연결 순서 (호출 체인)

1. `setup_stack`가 `stack_bottom` 계산.
2. `vm_alloc_page_with_initializer (VM_ANON, stack_bottom, true, ...)`로 SPT 등록.
3. `vm_claim_page (stack_bottom)`로 즉시 매핑.
4. 성공 시 `if_->rsp = USER_STACK` 설정 후 인자 스택 경로로 진행.

### 4.5 실패 처리/롤백 규칙

- SPT 등록 실패 시 즉시 `false`.
- `vm_claim_page` 실패 시 즉시 `false`.
- 실패한 경우 `if_->rsp`를 설정하지 않는다.
- D 범위에서 추가 stack growth 복구를 시도하지 않는다.

### 4.6 완료 체크리스트

- `setup_stack`에서 `stack_bottom` 계산이 정확하다.
- `VM_ANON` 스택 페이지가 SPT에 등록된다.
- 즉시 `vm_claim_page`로 매핑이 완료된다.
- 성공 시 `if_->rsp = USER_STACK`이 보장된다.
