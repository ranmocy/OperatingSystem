#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#define USER_ADDRESS_BOTTM ((void *) 0x08048000)

static void syscall_handler(struct intr_frame *);
static bool valid_addr(int);
static void check_valid_pointer(void *vaddr);


void
syscall_init(void)
{
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
	int *p = (int*)f->esp;
	switch (*p){
	case SYS_WRITE:
		if (!valid_addr(p[2]))
			printf("Oops\n");
		if (p[1] == 1)
			putbuf((const char*)p[2], p[3]);
		break;

	case SYS_HALT:
		shutdown_power_off();
		break;
	case SYS_EXEC:


	default:
		printf("Oops\n");
		thread_exit();
	}
}
static bool
valid_addr(int addr){
	// TODO
	return true;
}

static void
check_valid_pointer(const void *vaddr){
	if (!is_user_vaddr(vaddr) || vaddr < USER_ADDRESS_BOTTM){
		exit(ERROR);
	}
}

static int
vaddr_to_phyaddr(void *vaddr){
	// TO DO: verify the virtual memory exist, if exist return the physical address
	//        else exist with ERROR.
	check_valid_ptr(vaddr);
	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if (!ptr)
	{
		exit(ERROR);
	}
	return (int)ptr;
}