# Project 3 VM - SPT, VA, PA, KVA 흐름 정리

## 0. 이 문서의 목적

Project 3 Virtual Memory를 구현하기 전에 꼭 잡아야 하는 개념 흐름을 정리한다.

핵심 질문은 다음이다.

- `va`와 `pa`는 무엇인가?
- `pa`와 `kva`는 무엇이 다른가?
- 유저 프로그램이 메모리에 접근하면 커널은 무엇을 찾는가?
- SPT는 언제 쓰이는가?
- page와 frame은 언제 연결되는가?
- page table에 매핑된다는 것은 정확히 무엇인가?

---

## 1. 핵심 용어

### VA

`VA`는 `Virtual Address`, 즉 가상 주소다.

유저 프로그램이 사용하는 주소다.

```text
va = 유저 프로그램이 보는 주소
```

유저 프로그램은 실제 RAM 주소를 직접 알지 못한다.

유저 프로그램은 자기 입장에서의 주소, 즉 가상 주소만 사용한다.

---

### PA

`PA`는 `Physical Address`, 즉 물리 주소다.

실제 RAM 안에서 데이터가 위치하는 주소다.

```text
pa = 실제 물리 메모리 주소
```

CPU/MMU는 page table을 보고 `va`를 `pa`로 변환한다.

---

### KVA

`KVA`는 `Kernel Virtual Address`, 즉 커널 가상 주소다.

커널이 물리 frame에 접근하기 위해 사용하는 가상 주소다.

```text
kva = 커널이 물리 frame을 읽고 쓰기 위해 사용하는 주소
```

커널도 C 코드에서 메모리를 읽고 쓰려면 포인터를 사용한다.

그 포인터는 보통 물리 주소 `pa`가 아니라 커널 가상 주소 `kva`다.

---

## 2. VA, PA, KVA의 관계

세 주소의 관계는 다음처럼 정리할 수 있다.

```text
va  = 유저 프로그램이 사용하는 주소
pa  = 실제 RAM의 물리 주소
kva = 커널이 해당 물리 메모리에 접근하기 위해 사용하는 주소
```

유저 프로그램은 `va`를 사용한다.

커널은 frame 내용을 직접 채울 때 `kva`를 사용한다.

CPU/MMU는 page table을 보고 `va`를 실제 `pa`로 변환한다.

---

## 3. 왜 커널도 PA를 직접 쓰지 않고 KVA를 쓰는가?

커널도 CPU 위에서 실행되는 코드다.

가상 메모리 모드에서는 C 포인터로 접근하는 주소가 기본적으로 가상 주소로 해석된다.

따라서 커널이 물리 frame을 직접 읽고 쓰려면, 그 물리 frame이 커널 주소 공간에 매핑되어 있어야 한다.

이때 커널이 사용하는 주소가 `kva`다.

예를 들어 frame 하나를 얻으면 Pintos에서는 보통 다음처럼 커널이 접근 가능한 주소를 받는다.

```c
void *kva = palloc_get_page(PAL_USER);
```

이 `kva`는 커널이 해당 frame을 읽고 쓰기 위한 주소다.

이후 커널은 이 주소에 파일 내용을 읽어오거나, 0으로 채우거나, swap에서 데이터를 복원한다.

---

## 4. Page와 Frame의 차이

### Page

`page`는 유저 가상 주소 하나에 대한 커널의 관리 정보다.

Project 3에서는 보통 `struct page`로 표현된다.

```text
page = 어떤 va에 대한 커널의 관리 정보
```

page는 다음과 같은 정보를 가질 수 있다.

- 이 page의 유저 가상 주소
- writable 여부
- 현재 연결된 frame
- anonymous page인지 file-backed page인지
- lazy loading에 필요한 정보
- swap에 저장되어 있는지 여부

---

### Frame

`frame`은 실제 물리 메모리의 한 칸이다.

보통 한 frame은 한 page 크기와 같다.

```text
frame = 실제 RAM에 있는 4KB짜리 공간
```

Pintos에서는 frame을 커널이 접근할 때 `frame->kva`를 사용한다.

```c
struct frame {
    void *kva;
    struct page *page;
};
```

---

## 5. Page와 Frame은 항상 연결되어 있는가?

항상 연결되어 있지는 않다.

이 부분이 Project 3에서 중요하다.

### 아직 메모리에 올라오지 않은 page

lazy loading 상태이거나 swap out된 page는 `struct page`는 있지만 실제 frame은 없을 수 있다.

```text
page 있음
frame 없음
page->frame == NULL
```

이 경우 page는 "정상적인 가상 페이지 정보"만 가지고 있고, 실제 물리 메모리에는 아직 올라와 있지 않다.

---

### 이미 메모리에 올라온 page

page가 실제 frame에 올라오면 다음 연결이 생긴다.

```text
page->frame = frame
frame->page = page
```

이 상태에서는 page를 통해 현재 연결된 frame을 알 수 있다.

```text
page 있음
frame 있음
page->frame != NULL
```

---

## 6. SPT의 역할

SPT는 `Supplemental Page Table`, 즉 보조 페이지 테이블이다.

SPT는 `va`를 기준으로 `struct page`를 찾기 위한 커널 자료구조다.

```text
SPT: va -> struct page
```

기존 page table은 현재 실제로 매핑된 `va -> pa` 정보를 담는다.

하지만 Project 3에서는 아직 물리 메모리에 올라오지 않은 page도 관리해야 한다.

따라서 page table만으로는 부족하다.

SPT는 다음 정보를 판단하는 데 사용된다.

- 이 `va`가 정상적인 page인가?
- 아직 frame만 안 붙은 상태인가?
- lazy loading으로 나중에 파일에서 읽어와야 하는가?
- swap out된 page인가?
- writable page인가?

---

## 7. Page Table과 SPT의 차이

둘은 역할이 다르다.

### Page Table

page table은 CPU/MMU가 실제 주소 변환에 사용하는 표다.

```text
page table: va -> pa
```

page table에 매핑이 있으면 유저 프로그램은 page fault 없이 해당 주소에 접근할 수 있다.

---

### SPT

SPT는 커널이 page fault를 처리하기 위해 참고하는 보조 자료구조다.

```text
SPT: va -> struct page
```

SPT에 page가 있다는 것은 해당 `va`가 정상적으로 관리되는 가상 page라는 뜻이다.

하지만 SPT에 page가 있다고 해서 반드시 현재 물리 메모리에 올라와 있다는 뜻은 아니다.

---

## 8. Page Fault가 났을 때의 흐름

### Page Fault란?

page fault는 유저 프로그램이 어떤 `va`에 접근했는데, 현재 page table만으로는 그 주소를 실제 물리 메모리로 변환할 수 없을 때 발생하는 예외다.

즉, CPU/MMU가 page table을 확인했는데 다음과 같은 상황이면 page fault가 난다.

```text
이 va가 실제 pa로 매핑되어 있지 않다.
또는 접근 권한이 맞지 않는다.
```

Project 3에서 중요한 점은 page fault가 항상 에러는 아니라는 것이다.

아직 물리 메모리에 올라오지 않은 정상 page에 처음 접근해도 page fault가 날 수 있다.

이 경우 커널은 SPT를 확인해서 해당 `va`가 정상 page인지 판단하고, 정상 page라면 frame을 얻어 메모리에 올린 뒤 실행을 계속할 수 있게 만든다.

반대로 SPT에도 없는 주소이거나, 권한이 맞지 않는 접근이라면 잘못된 메모리 접근으로 보고 프로세스를 종료해야 한다.

---

유저 프로그램이 어떤 `va`에 접근했는데 page table에 매핑이 없으면 page fault가 발생한다.

그때 커널은 다음 흐름으로 처리한다.

```text
1. 유저 프로그램이 va에 접근한다.

2. page table에 va -> pa 매핑이 없다.

3. page fault가 발생한다.

4. 커널이 fault address, 즉 va를 얻는다.

5. 커널이 SPT에서 va에 해당하는 struct page를 찾는다.

6. page가 없으면 잘못된 주소로 판단한다.

7. page가 있으면 정상 page지만 아직 frame이 없을 수 있다고 판단한다.

8. 빈 frame을 얻는다.

9. page와 frame을 서로 연결한다.

10. frame->kva를 이용해 실제 frame에 데이터를 채운다.

11. page table에 va -> 해당 frame의 물리 위치를 매핑한다.

12. 같은 명령을 다시 실행하면 이번에는 page fault 없이 접근된다.
```

---

## 9. 핵심 흐름 한 줄 정리

```text
유저 va 접근
-> page fault
-> SPT에서 va로 struct page 찾기
-> frame 얻기
-> page와 frame 연결
-> frame->kva로 데이터 채우기
-> page table에 va -> pa 매핑
-> 다음 접근부터 바로 성공
```

---

## 10. "page로 frame을 찾는다"는 말의 정확한 의미

이 표현은 상황에 따라 맞을 수도 있고, 틀릴 수도 있다.

### 이미 frame에 올라온 page라면

이미 claim된 page는 `page->frame`을 가진다.

이 경우에는 page를 통해 frame을 찾을 수 있다.

```text
page -> frame
```

---

### 아직 frame에 올라오지 않은 page라면

lazy loading 상태의 page는 frame이 없을 수 있다.

이 경우에는 page로 frame을 찾는 것이 아니다.

정확한 표현은 다음이다.

```text
SPT에서 page를 찾고,
그 page를 올릴 빈 frame을 새로 얻어 연결한다.
```

즉, 처음부터 page가 frame을 가지고 있는 것이 아니다.

page fault 처리 과정에서 frame을 얻어 page에 붙인다.

---

## 11. "va와 frame을 매핑한다"는 말의 의미

이 표현도 두 단계로 나누어 이해해야 한다.

### 1단계: 커널 내부 구조체 연결

커널은 page와 frame을 서로 연결한다.

```text
page->frame = frame
frame->page = page
```

이 연결은 커널이 관리하기 위한 정보다.

---

### 2단계: page table 매핑

그 다음 page table에 `va -> pa` 매핑을 만든다.

Pintos 코드에서는 보통 다음 형태가 된다.

```c
pml4_set_page(thread_current()->pml4, page->va, frame->kva, writable);
```

여기서 `frame->kva`를 넘기지만, 결과적으로 page table에는 해당 frame의 물리 위치가 연결된다.

즉 다음부터는 CPU/MMU가 `va`를 실제 물리 frame으로 변환할 수 있다.

```text
va -> pa
```

---

## 12. 매핑 이후에는 어떻게 되는가?

page table에 `va -> pa` 매핑이 생기면 같은 주소에 다시 접근할 때는 page fault가 나지 않는다.

```text
유저가 va 접근
-> CPU/MMU가 page table 확인
-> pa로 변환
-> 실제 RAM 접근
```

즉, 최초 접근에서는 page fault가 발생할 수 있지만, 매핑이 완료된 후에는 바로 접근할 수 있다.

단, 나중에 eviction이 발생하면 이 매핑은 제거될 수 있다.

그 경우 같은 `va`에 다시 접근하면 다시 page fault가 발생하고, 커널은 SPT를 참고해 page를 다시 frame에 올린다.

---

## 13. SPT와 Page Table의 상태 비교

### SPT에 page가 있다

```text
이 va는 정상적으로 관리되는 가상 page다.
```

하지만 실제 메모리에 올라와 있다는 뜻은 아니다.

---

### page->frame이 있다

```text
이 page는 현재 실제 frame에 올라와 있다.
```

---

### page table에 va -> pa가 있다

```text
CPU/MMU가 이 va를 실제 물리 메모리로 바로 변환할 수 있다.
```

즉, page fault 없이 접근 가능하다.

---

## 14. 최종 정리

Project 3 VM의 초반 핵심은 다음 흐름을 이해하는 것이다.

```text
SPT는 va로 struct page를 찾기 위한 커널 자료구조다.

struct page는 가상 page의 상태와 로딩 정보를 담는다.

struct frame은 실제 물리 메모리 한 칸을 나타낸다.

page fault가 나면 커널은 SPT에서 page를 찾는다.

page가 있으면 frame을 얻어 page와 연결한다.

커널은 frame->kva로 실제 frame 내용을 채운다.

page table에는 va -> pa 매핑이 생긴다.

이후 같은 va 접근은 page fault 없이 성공한다.
```

---

## 15. 팀원에게 설명할 때의 핵심 문장

> 유저 프로그램은 page를 주는 것이 아니라 va에 접근한다.  
> page fault가 나면 커널은 그 va로 SPT에서 struct page를 찾는다.  
> page가 있으면 정상 page라고 보고, 빈 frame을 얻어 page와 연결한다.  
> 커널은 frame->kva로 실제 물리 frame에 데이터를 채운다.  
> 마지막으로 page table에 va -> pa 매핑을 만들어 다음 접근부터는 바로 메모리에 접근할 수 있게 한다.
