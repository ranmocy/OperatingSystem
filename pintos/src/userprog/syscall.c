#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static bool valid_addr(int);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
	int *p = (int*)f->esp;
	switch (*p){
	case SYS_WRITE:
		if (!valid_addr(p[2]))
			printf("Oops\n");
		if (p[1] == 1)
			putbuf((const char*)p[2], p[3]);
		break;
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
