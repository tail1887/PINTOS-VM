#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "lib/kernel/console.h"
#include "intrinsic.h"
#include "lib/kernel/stdio.h"
#include <string.h>
// 추가된 헤더파일
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "threads/vaddr.h"
#include "threads/mmu.h"

// 평소에는 꺼두기
#define USER_MEM_DEBUG 0
#if USER_MEM_DEBUG
#define user_mem_debug(...) printf(__VA_ARGS__)
#else
#define user_mem_debug(...) ((void) 0)
#endif

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

// 시스템콜 함수
static int sys_write(int fd, const void *buffer, unsigned size);
static void sys_exit(int status);

// 유저 메모리 유효성 검사 함수
static void fail_invalid_user_memory(void);
static bool is_valid_user_ptr(const void *uaddr);
static void validate_user_ptr(const void *uaddr);
static void validate_user_buffer(const void *buffer, size_t size);
static void validate_user_string(const char *str);



/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

// 유저 메모리 유효성 검사 함수들
// 실패 종료 경로 함수
static void
fail_invalid_user_memory(void) {
	sys_exit(-1);
}

// 접근 가능한 사용자 주소인지 판별하는 함수
static bool
is_valid_user_ptr(const void *uaddr) {

	// NULL 포인터를 실패 처리한다.
	if(uaddr == NULL){
		user_mem_debug("invalid user ptr: NULL\n");
		return false;
	}

	// is_user_vaddr()로 커널 주소를 차단한다.
	if(!is_user_vaddr((void *) uaddr)){
		user_mem_debug("invalid user ptr: kernel addr %p\n", uaddr);
		return false;
	}

	// 현재 thread의 page table에서 매핑 여부를 확인한다.
	if(pml4_get_page(thread_current()->pml4, (void *) uaddr) == NULL){
		user_mem_debug("invalid user ptr: unmapped %p\n", uaddr);
		return false;
	}

	return true;
}

//단일 포인터 검증 함수
static void
validate_user_ptr(const void *uaddr) {

}

static void
validate_user_buffer(const void *buffer, size_t size) {

}

static void
validate_user_string(const char *str) {

}


// 시스템 콜 함수들

static int sys_write(int fd, const void *buffer, unsigned size) {
	if (fd == 1) {
		putbuf(buffer, size);
		return size;
	}
	return 0;
}

static void
sys_exit(int status) {
    printf("%s: exit(%d)\n", thread_current()->name, status);
    thread_exit();
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.

	// 10번이 SYS_WRITE
	int sys_call = f->R.rax;

	switch (sys_call) {
		case SYS_WRITE:
			f->R.rax = sys_write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_EXIT:
			sys_exit(f->R.rdi);
	}

}